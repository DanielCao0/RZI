/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief USP backend for raw LoRa and FSK.
 *
 * P2P send and receive are radio access controller (RAC) transactions below
 * LoRaWAN priority, so they share the radio with an active LoRaWAN session.
 * Radio certification tests use the LoRa Basics Modem test mode, entered on
 * demand and left by the next P2P operation or by stop().
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <smtc_modem_hal.h>
#include <smtc_modem_test_api.h>
#include <smtc_rac_api.h>

#include "lora_backend.h"
#include "usp_platform.h"

/* The RAC access id is the priority; both sit below every LoRaWAN task. */
#define TX_ID RAC_LOW_PRIORITY
#define RX_ID RAC_VERY_LOW_PRIORITY

#define RX_CONTINUOUS 65535U
/* Continuous receive is a chain of windows of this length. */
#define RX_WINDOW_MS  60000U

/* smtc_modem_test_get_rssi() reports RSSI + 64. */
#define USP_TEST_RSSI_OFFSET 64

/* LoRa Basics Modem test-mode FSK packet format. */
#define FSK_PREAMBLE_DETECTOR RAL_GFSK_PREAMBLE_DETECTOR_MIN_16BITS
#define FSK_WHITENING_SEED    0x01FFU
#define FSK_CRC_SEED          0x1D0FU
#define FSK_CRC_POLYNOMIAL    0x1021U
static const uint8_t fsk_sync_word[] = {0xC1, 0x94, 0xC1};

struct rx_packet {
	uint8_t size;
	int16_t rssi_dbm;
	int8_t snr_quarter_db;
	uint8_t data[RZI_LORA_MAX_PAYLOAD];
};

K_MSGQ_DEFINE(rx_queue, sizeof(struct rx_packet), 2, 4);

/* Protected by the USP lock, which RAC callbacks also run under. */
static struct rzi_lora_config config;
static enum rzi_lora_modulation modulation = RZI_LORA_MOD_LORA;
static bool opened;
static bool test_mode;
static bool tx_busy;
static bool rx_active;
static bool rx_continuous;
static int64_t rx_deadline_ms;
static uint8_t tx_buffer[RZI_LORA_MAX_PAYLOAD];
static uint8_t rx_buffer[RZI_LORA_MAX_PAYLOAD];

/* Completed TX handed to poll(): tx_error is valid once tx_done is set. */
static atomic_t tx_done;
static atomic_t tx_error;

static int rac_result(smtc_rac_return_code_t rc)
{
	switch (rc) {
	case SMTC_RAC_SUCCESS:
		return 0;
	case SMTC_RAC_BUSY:
		return -RZI_ERR_BUSY;
	case SMTC_RAC_INVALID_PARAMETER:
		return -RZI_ERR_INVALID;
	case SMTC_RAC_NOT_SUPPORTED:
	case SMTC_RAC_NOT_IMPLEMENTED:
		return -RZI_ERR_NOT_SUPPORTED;
	case SMTC_RAC_NOT_INITIALIZED:
		return -RZI_ERR_NOT_READY;
	case SMTC_RAC_TIMEOUT:
		return -RZI_ERR_TIMEOUT;
	default:
		return -RZI_ERR_IO;
	}
}

static ral_lora_bw_t map_bandwidth(uint32_t index)
{
	static const ral_lora_bw_t table[] = {
		RAL_LORA_BW_125_KHZ, RAL_LORA_BW_250_KHZ, RAL_LORA_BW_500_KHZ, RAL_LORA_BW_007_KHZ,
		RAL_LORA_BW_010_KHZ, RAL_LORA_BW_015_KHZ, RAL_LORA_BW_020_KHZ, RAL_LORA_BW_031_KHZ,
		RAL_LORA_BW_041_KHZ, RAL_LORA_BW_062_KHZ,
	};

	if (index >= ARRAY_SIZE(table)) {
		return RAL_LORA_BW_125_KHZ;
	}
	return table[index];
}

/* SX126x two-byte sync words (0x3444, 0x1424) carry the LoRa byte in their high nibbles. */
static uint8_t lora_sync_byte(uint16_t sync_word)
{
	if (sync_word > 0xFFU) {
		return (uint8_t)(((sync_word >> 8) & 0xF0U) | ((sync_word >> 4) & 0x0FU));
	}
	return (uint8_t)sync_word;
}

static smtc_modem_test_mode_sync_word_t test_sync(uint16_t sync_word)
{
	switch (lora_sync_byte(sync_word)) {
	case 0x12U:
		return SYNC_WORD_0x12;
	case 0x21U:
		return SYNC_WORD_0x21;
	case 0x56U:
		return SYNC_WORD_0x56;
	default:
		return SYNC_WORD_0x34;
	}
}

static void fill_lora(smtc_rac_context_t *ctx, bool is_tx, uint16_t size, uint32_t timeout_ms)
{
	smtc_rac_radio_lora_params_t *lora = &ctx->radio_params.lora;

	ctx->modulation_type = SMTC_RAC_MODULATION_LORA;
	memset(lora, 0, sizeof(*lora));
	lora->is_tx = is_tx;
	lora->frequency_in_hz = config.frequency_hz;
	lora->sf = (ral_lora_sf_t)config.spreading_factor;
	lora->bw = map_bandwidth(config.bandwidth);
	lora->cr = (ral_lora_cr_t)(config.coding_rate + 1U);
	lora->preamble_len_in_symb = config.preamble_length;
	lora->header_type = config.fixed_length ? RAL_LORA_PKT_IMPLICIT : RAL_LORA_PKT_EXPLICIT;
	lora->invert_iq_is_on = config.iq_inverted ? 1U : 0U;
	lora->crc_is_on = 1U;
	lora->sync_word = (smtc_rac_lora_syncword_t)lora_sync_byte(config.sync_word);
	lora->tx_power_in_dbm = (uint8_t)config.tx_power_dbm;
	lora->tx_size = size;
	lora->rx_timeout_ms = timeout_ms;
	lora->symb_nb_timeout = config.symbol_timeout;
	lora->max_rx_size = sizeof(rx_buffer);
}

static void fill_fsk(smtc_rac_context_t *ctx, bool is_tx, uint16_t size, uint32_t timeout_ms)
{
	smtc_rac_radio_fsk_params_t *fsk = &ctx->radio_params.fsk;
	/* Carson bandwidth when the configuration holds a LoRa index, not hertz. */
	uint32_t bandwidth = config.bandwidth >= 4800U
				     ? config.bandwidth
				     : 2U * config.fsk_deviation + config.fsk_bitrate;

	ctx->modulation_type = SMTC_RAC_MODULATION_FSK;
	memset(fsk, 0, sizeof(*fsk));
	fsk->is_tx = is_tx;
	fsk->tx_size = size;
	fsk->max_rx_size = sizeof(rx_buffer);
	fsk->frequency_in_hz = config.frequency_hz;
	fsk->tx_power_in_dbm = config.tx_power_dbm;
	fsk->br_in_bps = config.fsk_bitrate;
	fsk->fdev_in_hz = config.fsk_deviation;
	fsk->bw_dsb_in_hz = bandwidth;
	fsk->pulse_shape = RAL_GFSK_PULSE_SHAPE_BT_1;
	fsk->header_type = config.fixed_length ? RAL_GFSK_PKT_FIX_LEN : RAL_GFSK_PKT_VAR_LEN;
	fsk->preamble_len_in_bits = config.preamble_length * 8U;
	fsk->preamble_detector = FSK_PREAMBLE_DETECTOR;
	fsk->sync_word_len_in_bits = sizeof(fsk_sync_word) * 8U;
	fsk->crc_type = RAL_GFSK_CRC_2_BYTES_INV;
	fsk->dc_free = RAL_GFSK_DC_FREE_WHITENING;
	fsk->sync_word = fsk_sync_word;
	fsk->whitening_seed = FSK_WHITENING_SEED;
	fsk->crc_seed = FSK_CRC_SEED;
	fsk->crc_polynomial = FSK_CRC_POLYNOMIAL;
	fsk->rx_timeout_ms = timeout_ms;
}

static int submit(uint8_t id, bool is_tx, uint16_t size, uint32_t timeout_ms)
{
	smtc_rac_context_t *ctx = smtc_rac_get_context(id);

	if (modulation == RZI_LORA_MOD_FSK) {
		fill_fsk(ctx, is_tx, size, timeout_ms);
	} else {
		fill_lora(ctx, is_tx, size, timeout_ms);
	}
	ctx->scheduler_config.scheduling = SMTC_RAC_ASAP_TRANSACTION;
	ctx->scheduler_config.start_time_ms = smtc_modem_hal_get_time_in_ms();
	return rac_result(smtc_rac_submit_radio_transaction(id));
}

static void on_tx_done(rp_status_t status)
{
	int error;

	switch (status) {
	case RP_STATUS_TX_DONE:
		error = 0;
		break;
	case RP_STATUS_TASK_ABORTED:
		/* Preempted by LoRaWAN or cancelled by stop(). */
		error = -RZI_ERR_BUSY;
		break;
	default:
		error = -RZI_ERR_IO;
		break;
	}
	tx_busy = false;
	atomic_set(&tx_error, error);
	atomic_set(&tx_done, 1);
}

static int submit_rx_window(void)
{
	uint32_t window_ms = RX_WINDOW_MS;

	if (!rx_continuous) {
		int64_t remaining = rx_deadline_ms - k_uptime_get();

		if (remaining <= 0) {
			return -RZI_ERR_TIMEOUT;
		}
		window_ms = (uint32_t)remaining;
	}
	return submit(RX_ID, false, 0U, window_ms);
}

static void on_rx_done(rp_status_t status)
{
	const smtc_rac_context_t *ctx = smtc_rac_get_context(RX_ID);
	bool again;

	if (status == RP_STATUS_RX_PACKET && ctx->smtc_rac_data_result.rx_size > 0U) {
		struct rx_packet packet = {
			.size = MIN(ctx->smtc_rac_data_result.rx_size, sizeof(packet.data)),
			.rssi_dbm = ctx->smtc_rac_data_result.rssi_result,
			.snr_quarter_db = (int8_t)(ctx->smtc_rac_data_result.snr_result * 4),
		};

		memcpy(packet.data, rx_buffer, packet.size);
		(void)k_msgq_put(&rx_queue, &packet, K_NO_WAIT);
	}
	if (!rx_active) {
		return;
	}
	/*
	 * A timed receive ends with its first packet or its timeout. Windows
	 * cut short by higher-priority radio use resume until the deadline.
	 */
	again = rx_continuous || status == RP_STATUS_TASK_ABORTED;
	if (!again || submit_rx_window() != 0) {
		rx_active = false;
	}
}

static int open_radio(void)
{
	smtc_rac_context_t *ctx;

	if (opened) {
		return 0;
	}
	if (smtc_rac_open_radio(TX_ID) != TX_ID || smtc_rac_open_radio(RX_ID) != RX_ID) {
		return -RZI_ERR_NO_RESOURCE;
	}
	ctx = smtc_rac_get_context(TX_ID);
	ctx->smtc_rac_data_buffer_setup.tx_payload_buffer = tx_buffer;
	ctx->smtc_rac_data_buffer_setup.size_of_tx_payload_buffer = sizeof(tx_buffer);
	ctx->scheduler_config.callback_post_radio_transaction = on_tx_done;
	ctx = smtc_rac_get_context(RX_ID);
	ctx->smtc_rac_data_buffer_setup.rx_payload_buffer = rx_buffer;
	ctx->smtc_rac_data_buffer_setup.size_of_rx_payload_buffer = sizeof(rx_buffer);
	ctx->scheduler_config.callback_post_radio_transaction = on_rx_done;
	opened = true;
	return 0;
}

/* With the lock held: cancel P2P transactions. */
static void abort_p2p(void)
{
	rx_active = false;
	if (opened) {
		(void)smtc_rac_abort_radio_submit(RX_ID);
		(void)smtc_rac_abort_radio_submit(TX_ID);
	}
}

/* With the lock held. Test mode suspends LoRaWAN until leave_test_mode(). */
static int enter_test_mode(void)
{
	int rc;

	if (test_mode) {
		return 0;
	}
	abort_p2p();
	rc = rzi_usp_result(smtc_modem_test_start());
	if (rc == 0) {
		test_mode = true;
	}
	return rc;
}

static int leave_test_mode(void)
{
	int rc;

	if (!test_mode) {
		return 0;
	}
	rc = rzi_usp_result(smtc_modem_test_stop());
	if (rc == 0) {
		test_mode = false;
	}
	return rc;
}

static int usp_start(enum rzi_lora_modulation value)
{
	int rc;

	rzi_usp_init();
	rzi_usp_lock();
	rc = open_radio();
	if (rc == 0) {
		modulation = value;
	}
	rzi_usp_unlock();
	return rc;
}

static int usp_stop(void)
{
	int rc;

	rzi_usp_lock();
	abort_p2p();
	tx_busy = false;
	rc = leave_test_mode();
	rzi_usp_unlock();
	atomic_clear(&tx_done);
	k_msgq_purge(&rx_queue);
	return rc;
}

static int usp_apply(const struct rzi_lora_config *next)
{
	rzi_usp_lock();
	config = *next;
	rzi_usp_unlock();
	return 0;
}

static int usp_send(const uint8_t *data, size_t size)
{
	int rc;

	rzi_usp_lock();
	rc = leave_test_mode();
	if (rc == 0 && tx_busy) {
		rc = -RZI_ERR_BUSY;
	}
	if (rc == 0) {
		memcpy(tx_buffer, data, size);
		rc = submit(TX_ID, true, (uint16_t)size, 0U);
		tx_busy = rc == 0;
	}
	rzi_usp_unlock();
	return rc;
}

static int usp_receive(uint32_t timeout_ms)
{
	int rc;

	rzi_usp_lock();
	rx_active = false;
	(void)smtc_rac_abort_radio_submit(RX_ID);
	if (timeout_ms == 0U) {
		rzi_usp_unlock();
		return 0;
	}
	rc = leave_test_mode();
	if (rc == 0) {
		rx_continuous = timeout_ms == RX_CONTINUOUS;
		rx_deadline_ms = k_uptime_get() + timeout_ms;
		rc = submit_rx_window();
		rx_active = rc == 0;
	}
	rzi_usp_unlock();
	return rc;
}

static int usp_test_rssi(void)
{
	int rc;

	rzi_usp_lock();
	rc = enter_test_mode();
	if (rc == 0) {
		rc = rzi_usp_result(smtc_modem_test_rssi_lbt(config.frequency_hz, 125000U, 100U));
	}
	rzi_usp_unlock();
	return rc;
}

static int usp_test_get_rssi(int16_t *rssi_dbm)
{
	int8_t raw = 0;
	int rc = -RZI_ERR_NOT_READY;

	rzi_usp_lock();
	if (test_mode) {
		rc = rzi_usp_result(smtc_modem_test_get_rssi(&raw));
	}
	rzi_usp_unlock();
	if (rc == 0) {
		*rssi_dbm = (int16_t)raw - USP_TEST_RSSI_OFFSET;
	}
	return rc;
}

static int usp_test_cw(uint32_t frequency_hz, int8_t power_dbm, uint32_t duration_ms)
{
	int rc;

	ARG_UNUSED(duration_ms);
	rzi_usp_lock();
	rc = enter_test_mode();
	if (rc == 0) {
		rc = rzi_usp_result(smtc_modem_test_tx_cw(frequency_hz, power_dbm));
	}
	rzi_usp_unlock();
	return rc;
}

static int usp_test_tone(void)
{
	struct rzi_lora_config current;

	rzi_usp_lock();
	current = config;
	rzi_usp_unlock();
	return usp_test_cw(current.frequency_hz, current.tx_power_dbm, 0U);
}

static int usp_test_tx(uint32_t packet_count)
{
	uint8_t payload[16] = {0};
	int rc;

	rzi_usp_lock();
	rc = enter_test_mode();
	if (rc == 0) {
		rc = rzi_usp_result(smtc_modem_test_tx_lora(
			payload, sizeof(payload), config.frequency_hz, config.tx_power_dbm,
			(ral_lora_sf_t)config.spreading_factor, map_bandwidth(config.bandwidth),
			(ral_lora_cr_t)(config.coding_rate + 1U), test_sync(config.sync_word),
			config.iq_inverted, true, RAL_LORA_PKT_EXPLICIT, config.preamble_length,
			packet_count, 0U));
	}
	rzi_usp_unlock();
	return rc;
}

static int usp_test_rx(uint32_t packet_count)
{
	ARG_UNUSED(packet_count);
	return usp_receive(RX_CONTINUOUS);
}

static int usp_test_stop(void)
{
	int rc = 0;

	rzi_usp_lock();
	if (test_mode) {
		rc = rzi_usp_result(smtc_modem_test_nop(true));
	}
	rzi_usp_unlock();
	return rc;
}

static void usp_poll(void)
{
	struct rx_packet packet;

	if (atomic_cas(&tx_done, 1, 0)) {
		rzi_lora_publish_tx_done((int)atomic_get(&tx_error));
	}
	while (k_msgq_get(&rx_queue, &packet, K_NO_WAIT) == 0) {
		rzi_lora_publish_rx(packet.data, packet.size, packet.rssi_dbm,
				    packet.snr_quarter_db);
	}
}

const struct rzi_lora_backend_api rzi_lora_backend = {
	.start = usp_start,
	.stop = usp_stop,
	.apply_config = usp_apply,
	.send = usp_send,
	.receive = usp_receive,
	.test_rssi = usp_test_rssi,
	.test_get_rssi = usp_test_get_rssi,
	.test_tone = usp_test_tone,
	.test_tx = usp_test_tx,
	.test_rx = usp_test_rx,
	.test_cw = usp_test_cw,
	.test_stop = usp_test_stop,
	.poll = usp_poll,
};
