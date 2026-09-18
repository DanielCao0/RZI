/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief USP LoRa Basics Modem test-API backend for raw LoRa and FSK.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <smtc_modem_test_api.h>

#include "lora_backend.h"

static enum rzi_lora_modulation modulation = RZI_LORA_MOD_LORA;
static bool test_mode;

static int map_rc(smtc_modem_return_code_t rc)
{
	switch (rc) {
	case SMTC_MODEM_RC_OK:
		return 0;
	case SMTC_MODEM_RC_NOT_INIT:
		return -EAGAIN;
	case SMTC_MODEM_RC_INVALID:
		return -EINVAL;
	case SMTC_MODEM_RC_BUSY:
		return -EBUSY;
	case SMTC_MODEM_RC_FAIL:
		return -EIO;
	default:
		return -EIO;
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

static smtc_modem_test_mode_sync_word_t map_sync(uint16_t sync_word)
{
	switch (sync_word & 0xffU) {
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

static int ensure_test_mode(void)
{
	int rc;

	if (test_mode) {
		return 0;
	}
	rc = map_rc(smtc_modem_test_start());
	if (rc == 0) {
		test_mode = true;
	}
	return rc;
}

static int usp_start(enum rzi_lora_modulation value)
{
	modulation = value;
	return ensure_test_mode();
}

static int usp_stop(void)
{
	int rc = 0;

	if (test_mode) {
		rc = map_rc(smtc_modem_test_stop());
		test_mode = false;
	}
	return rc;
}

static int usp_apply(const struct rzi_lora_config *config)
{
	ARG_UNUSED(config);
	return 0;
}

static int usp_send(const uint8_t *data, size_t size)
{
	struct rzi_lora_config config;
	int rc;

	rc = rzi_lora_get_config(&config);
	if (rc != 0) {
		return rc;
	}
	rc = ensure_test_mode();
	if (rc != 0) {
		return rc;
	}
	if (modulation == RZI_LORA_MOD_FSK) {
		rc = map_rc(smtc_modem_test_tx_fsk((uint8_t *)data, (uint8_t)size,
						   config.frequency_hz, config.tx_power_dbm, 1U,
						   0U));
	} else {
		rc = map_rc(smtc_modem_test_tx_lora(
			(uint8_t *)data, (uint8_t)size, config.frequency_hz, config.tx_power_dbm,
			(ral_lora_sf_t)config.spreading_factor, map_bandwidth(config.bandwidth),
			(ral_lora_cr_t)(config.coding_rate + 1U), map_sync(config.sync_word),
			config.iq_inverted, true,
			config.fixed_length ? RAL_LORA_PKT_IMPLICIT : RAL_LORA_PKT_EXPLICIT,
			config.preamble_length, 1U, 0U));
	}
	if (rc == 0) {
		rzi_lora_publish_tx_done(0);
	}
	return rc;
}

static int usp_receive(uint32_t timeout_ms)
{
	struct rzi_lora_config config;
	int rc;

	if (timeout_ms == 0U) {
		return map_rc(smtc_modem_test_nop(false));
	}
	rc = rzi_lora_get_config(&config);
	if (rc != 0) {
		return rc;
	}
	rc = ensure_test_mode();
	if (rc != 0) {
		return rc;
	}
	if (modulation == RZI_LORA_MOD_FSK) {
		return map_rc(smtc_modem_test_rx_fsk_continuous(config.frequency_hz));
	}
	return map_rc(smtc_modem_test_rx_lora(
		config.frequency_hz, (ral_lora_sf_t)config.spreading_factor,
		map_bandwidth(config.bandwidth), (ral_lora_cr_t)(config.coding_rate + 1U),
		map_sync(config.sync_word), config.iq_inverted, true,
		config.fixed_length ? RAL_LORA_PKT_IMPLICIT : RAL_LORA_PKT_EXPLICIT,
		config.preamble_length, config.symbol_timeout));
}

static int usp_test_rssi(void)
{
	struct rzi_lora_config config;
	int rc = rzi_lora_get_config(&config);

	if (rc != 0) {
		return rc;
	}
	rc = ensure_test_mode();
	if (rc != 0) {
		return rc;
	}
	return map_rc(smtc_modem_test_rssi_lbt(config.frequency_hz, 125000U, 100U));
}

static int usp_test_tone(void)
{
	struct rzi_lora_config config;
	int rc = rzi_lora_get_config(&config);

	if (rc != 0) {
		return rc;
	}
	rc = ensure_test_mode();
	if (rc != 0) {
		return rc;
	}
	return map_rc(smtc_modem_test_tx_cw(config.frequency_hz, config.tx_power_dbm));
}

static int usp_test_tx(uint32_t packet_count)
{
	uint8_t payload[16] = {0};
	struct rzi_lora_config config;
	int rc = rzi_lora_get_config(&config);

	if (rc != 0) {
		return rc;
	}
	rc = ensure_test_mode();
	if (rc != 0) {
		return rc;
	}
	return map_rc(smtc_modem_test_tx_lora(
		payload, sizeof(payload), config.frequency_hz, config.tx_power_dbm,
		(ral_lora_sf_t)config.spreading_factor, map_bandwidth(config.bandwidth),
		(ral_lora_cr_t)(config.coding_rate + 1U), map_sync(config.sync_word),
		config.iq_inverted, true, RAL_LORA_PKT_EXPLICIT, config.preamble_length,
		packet_count, 0U));
}

static int usp_test_rx(uint32_t packet_count)
{
	ARG_UNUSED(packet_count);
	return usp_receive(65535U);
}

static int usp_test_cw(uint32_t frequency_hz, int8_t power_dbm, uint32_t duration_ms)
{
	int rc;

	ARG_UNUSED(duration_ms);
	rc = ensure_test_mode();
	if (rc != 0) {
		return rc;
	}
	return map_rc(smtc_modem_test_tx_cw(frequency_hz, power_dbm));
}

static int usp_test_stop(void)
{
	if (!test_mode) {
		return 0;
	}
	return map_rc(smtc_modem_test_nop(true));
}

static void usp_poll(void)
{
	uint8_t payload[RZI_LORA_MAX_PAYLOAD];
	uint8_t size = 0;
	int16_t rssi = 0;
	int16_t snr = 0;

	if (!test_mode) {
		return;
	}
	if (smtc_modem_test_get_last_rx_packets(&rssi, &snr, payload, &size) == SMTC_MODEM_RC_OK &&
	    size > 0U) {
		rzi_lora_publish_rx(payload, size, rssi, (int8_t)(snr * 4));
	}
}

const struct rzi_lora_backend_api rzi_lora_backend = {
	.start = usp_start,
	.stop = usp_stop,
	.apply_config = usp_apply,
	.send = usp_send,
	.receive = usp_receive,
	.test_rssi = usp_test_rssi,
	.test_tone = usp_test_tone,
	.test_tx = usp_test_tx,
	.test_rx = usp_test_rx,
	.test_cw = usp_test_cw,
	.test_stop = usp_test_stop,
	.poll = usp_poll,
};
