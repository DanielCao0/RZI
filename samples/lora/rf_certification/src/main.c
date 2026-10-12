/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <rzi/lora/lora.h>
#include <rzi/lorawan/lorawan.h>

LOG_MODULE_REGISTER(rzi_rf_cert_sample, LOG_LEVEL_INF);

#define READY_TIMEOUT    K_SECONDS(10)
#define TX_TIMEOUT_MS    20000
#define POLL_INTERVAL_MS 5
#define PROGRESS_LOG_MS  10000
#define HEADER           "RF_CERT"
#define HEADER_SIZE      (sizeof(HEADER) - 1)
#define COUNTER_SIZE     4
#define LORA_BW_125_KHZ  0U
#define LORA_BW_250_KHZ  1U
#define LORA_BW_500_KHZ  2U
#define LORA_CR_4_5      0U
#define PREAMBLE_LENGTH  8U
#define FULL_PAYLOAD     RZI_LORA_MAX_PAYLOAD

/* FCC sweeps hop over 200 kHz channels from 902.3 MHz. */
#define FCC_HOP_BASE_HZ     902300000U
#define FCC_HOP_STEP_HZ     200000U
#define FCC_FHSS_CHANNELS   64U
#define FCC_HYBRID_CHANNELS 8U

/* US915 maximum application payload per data rate. */
#define US915_DR0_PAYLOAD 24
#define US915_DR1_PAYLOAD 66
#define US915_DR2_PAYLOAD 138
#define US915_DR3_PAYLOAD 235
#define US915_DR4_PAYLOAD 235

BUILD_ASSERT(HEADER_SIZE + COUNTER_SIZE <= US915_DR0_PAYLOAD);

struct cert_step {
	enum rzi_lora_modulation modulation;
	/* Zero for a fixed frequency, otherwise the hop channel count. */
	uint8_t hop_channels;
	uint32_t frequency_hz;
	uint8_t spreading_factor;
	uint8_t bandwidth;
	int8_t power_dbm;
	uint8_t payload_size;
};

#define LORA(freq, sf, bw, pwr, len) {RZI_LORA_MOD_LORA, 0U, (freq), (sf), (bw), (pwr), (len)}
#define FSK(freq, pwr)               {RZI_LORA_MOD_FSK, 0U, (freq), 0U, 0U, (pwr), FULL_PAYLOAD}
#define SWEEP(channels, sf, len)                                                                   \
	{RZI_LORA_MOD_LORA, (channels), FCC_HOP_BASE_HZ, (sf), LORA_BW_125_KHZ, 22, (len)}

#if defined(CONFIG_APP_RF_CERT_REGION_FCC)
#define REGION_NAME "FCC"
static const struct cert_step steps[] = {
	/* DTS mode */
	LORA(903000000U, 8, LORA_BW_500_KHZ, 22, US915_DR4_PAYLOAD),
	LORA(909400000U, 8, LORA_BW_500_KHZ, 22, US915_DR4_PAYLOAD),
	LORA(914200000U, 8, LORA_BW_500_KHZ, 22, US915_DR4_PAYLOAD),
	/* Hybrid mode */
	LORA(902300000U, 7, LORA_BW_125_KHZ, 22, US915_DR3_PAYLOAD),
	LORA(902300000U, 10, LORA_BW_125_KHZ, 22, US915_DR0_PAYLOAD),
	LORA(908700000U, 7, LORA_BW_125_KHZ, 22, US915_DR3_PAYLOAD),
	LORA(908700000U, 10, LORA_BW_125_KHZ, 22, US915_DR0_PAYLOAD),
	LORA(914900000U, 7, LORA_BW_125_KHZ, 22, US915_DR3_PAYLOAD),
	LORA(914900000U, 10, LORA_BW_125_KHZ, 22, US915_DR0_PAYLOAD),
	/* FHSS sweep */
	SWEEP(FCC_FHSS_CHANNELS, 10, US915_DR0_PAYLOAD),
	SWEEP(FCC_FHSS_CHANNELS, 9, US915_DR1_PAYLOAD),
	SWEEP(FCC_FHSS_CHANNELS, 8, US915_DR2_PAYLOAD),
	SWEEP(FCC_FHSS_CHANNELS, 7, US915_DR3_PAYLOAD),
	/* Hybrid sweep, sub-band 0 */
	SWEEP(FCC_HYBRID_CHANNELS, 10, US915_DR0_PAYLOAD),
	SWEEP(FCC_HYBRID_CHANNELS, 9, US915_DR1_PAYLOAD),
	SWEEP(FCC_HYBRID_CHANNELS, 8, US915_DR2_PAYLOAD),
	SWEEP(FCC_HYBRID_CHANNELS, 7, US915_DR3_PAYLOAD),
};
#elif defined(CONFIG_APP_RF_CERT_REGION_ARIB)
#define REGION_NAME "ARIB"
static const struct cert_step steps[] = {
	LORA(920600000U, 7, LORA_BW_125_KHZ, 9, FULL_PAYLOAD),
	LORA(920600000U, 12, LORA_BW_125_KHZ, 9, FULL_PAYLOAD),
	LORA(922000000U, 7, LORA_BW_125_KHZ, 9, FULL_PAYLOAD),
	LORA(922000000U, 12, LORA_BW_125_KHZ, 9, FULL_PAYLOAD),
	LORA(923400000U, 7, LORA_BW_125_KHZ, 9, FULL_PAYLOAD),
	LORA(923400000U, 12, LORA_BW_125_KHZ, 9, FULL_PAYLOAD),
	FSK(920600000U, 9),
	FSK(922000000U, 9),
	FSK(923400000U, 9),
};
#else
#define REGION_NAME "ETSI"
static const struct cert_step steps[] = {
	LORA(863100000U, 7, LORA_BW_125_KHZ, 14, FULL_PAYLOAD),
	LORA(863100000U, 12, LORA_BW_125_KHZ, 14, FULL_PAYLOAD),
	LORA(868300000U, 7, LORA_BW_125_KHZ, 14, FULL_PAYLOAD),
	LORA(868300000U, 12, LORA_BW_125_KHZ, 14, FULL_PAYLOAD),
	LORA(868500000U, 7, LORA_BW_125_KHZ, 14, FULL_PAYLOAD),
	LORA(868500000U, 12, LORA_BW_125_KHZ, 14, FULL_PAYLOAD),
	LORA(868300000U, 7, LORA_BW_250_KHZ, 14, FULL_PAYLOAD),
	LORA(866500000U, 7, LORA_BW_125_KHZ, 14, FULL_PAYLOAD),
	LORA(866500000U, 12, LORA_BW_125_KHZ, 14, FULL_PAYLOAD),
	FSK(863100000U, 14),
	FSK(866500000U, 14),
	FSK(868100000U, 14),
	FSK(868300000U, 14),
	FSK(868500000U, 14),
	FSK(869900000U, 14),
};
#endif

BUILD_ASSERT(CONFIG_APP_RF_CERT_ONLY_STEP <= ARRAY_SIZE(steps),
	     "CONFIG_APP_RF_CERT_ONLY_STEP is past the last step of this region");

static K_SEM_DEFINE(stack_ready, 0, 1);
static K_SEM_DEFINE(tx_done, 0, 1);

static int tx_error;
static uint8_t payload[FULL_PAYLOAD];
static uint8_t hop_order[FCC_FHSS_CHANNELS];

static void on_event(const struct rzi_lorawan_event *event, void *user_data)
{
	ARG_UNUSED(user_data);
	if (event->type == RZI_LORAWAN_EVENT_READY ||
	    (event->type == RZI_LORAWAN_EVENT_STATE_CHANGED &&
	     event->state == RZI_LORAWAN_STATE_READY)) {
		k_sem_give(&stack_ready);
	}
}

static const struct rzi_lorawan_callbacks lorawan_callbacks = {
	.on_event = on_event,
};

static void on_tx_done(int error, void *user_data)
{
	ARG_UNUSED(user_data);
	tx_error = error;
	k_sem_give(&tx_done);
}

static const struct rzi_lora_callbacks lora_callbacks = {
	.tx_done = on_tx_done,
};

/* The USP backend delivers tx_done() from rzi_lora_poll(). */
static bool wait_for(struct k_sem *sem, uint32_t timeout_ms)
{
	int64_t deadline = k_uptime_get() + timeout_ms;

	do {
		rzi_lora_poll();
		if (k_sem_take(sem, K_MSEC(POLL_INTERVAL_MS)) == 0) {
			return true;
		}
	} while (k_uptime_get() < deadline);
	return false;
}

/* The USP raw LoRa backend runs on the LoRa Basics Modem test API, so the
 * modem must be started, but not joined, before rzi_lora_start().
 */
static int start_radio(void)
{
	rzi_lorawan_callback_handle_t handle;
	int rc = rzi_lorawan_register_callbacks(&lorawan_callbacks, &handle);

	if (rc == 0) {
		rc = rzi_lorawan_start();
	}
	if (rc != 0) {
		LOG_ERR("Modem start failed: %d", rc);
		return rc;
	}
	if (k_sem_take(&stack_ready, READY_TIMEOUT) != 0) {
		LOG_ERR("Modem did not become ready");
		return -RZI_ERR_TIMEOUT;
	}
	rc = rzi_lora_register_callbacks(&lora_callbacks);
	if (rc != 0) {
		LOG_ERR("Raw LoRa callbacks failed: %d", rc);
	}
	return rc;
}

static void log_step(size_t index)
{
	static const uint16_t bw_khz[] = {125, 250, 500};
	const struct cert_step *step = &steps[index];
	uint32_t f = step->frequency_hz;

	if (step->modulation == RZI_LORA_MOD_FSK) {
		LOG_INF("%2zu  FSK   %3u.%03u MHz  50 kbps     %2d dBm  %3u B", index + 1U,
			f / 1000000U, (f / 1000U) % 1000U, step->power_dbm, step->payload_size);
	} else if (step->hop_channels != 0U) {
		LOG_INF("%2zu  LoRa  hop %2u ch from %3u.%03u MHz  SF%-2u BW%u  %2d dBm  %3u B",
			index + 1U, step->hop_channels, f / 1000000U, (f / 1000U) % 1000U,
			step->spreading_factor, bw_khz[step->bandwidth], step->power_dbm,
			step->payload_size);
	} else {
		LOG_INF("%2zu  LoRa  %3u.%03u MHz  SF%-2u BW%u  %2d dBm  %3u B", index + 1U,
			f / 1000000U, (f / 1000U) % 1000U, step->spreading_factor,
			bw_khz[step->bandwidth], step->power_dbm, step->payload_size);
	}
}

static int configure(const struct cert_step *step, uint32_t frequency_hz)
{
	struct rzi_lora_config config;
	int rc = rzi_lora_get_config(&config);

	if (rc != 0) {
		return rc;
	}
	config.frequency_hz = frequency_hz;
	config.tx_power_dbm = step->power_dbm;
	config.coding_rate = LORA_CR_4_5;
	config.preamble_length = PREAMBLE_LENGTH;
	if (step->modulation == RZI_LORA_MOD_LORA) {
		config.spreading_factor = step->spreading_factor;
		config.bandwidth = step->bandwidth;
	}
	return rzi_lora_set_config(&config);
}

static void shuffle_hops(uint8_t channels)
{
	for (uint8_t i = 0; i < channels; ++i) {
		hop_order[i] = i;
	}
	for (uint8_t i = channels - 1U; i > 0U; --i) {
		uint8_t j = sys_rand32_get() % (i + 1U);
		uint8_t tmp = hop_order[i];

		hop_order[i] = hop_order[j];
		hop_order[j] = tmp;
	}
}

static int send_packet(uint32_t counter, uint8_t size)
{
	int rc;

	sys_put_be32(counter, &payload[size - COUNTER_SIZE]);
	k_sem_reset(&tx_done);
	rc = rzi_lora_send(payload, size);
	if (rc != 0) {
		LOG_ERR("Packet %u rejected: %d", counter, rc);
		return rc;
	}
	if (!wait_for(&tx_done, TX_TIMEOUT_MS)) {
		LOG_ERR("Packet %u not sent", counter);
		return -RZI_ERR_TIMEOUT;
	}
	if (tx_error != 0) {
		LOG_ERR("Packet %u failed: %d", counter, tx_error);
	}
	return tx_error;
}

/* Transmit back to back on one step. A duration of zero never returns
 * unless a packet fails.
 */
static int run_step(size_t index, uint32_t duration_ms, uint32_t *counter)
{
	const struct cert_step *step = &steps[index];
	int64_t start = k_uptime_get();
	int64_t next_log = start + PROGRESS_LOG_MS;
	uint32_t sent = 0;
	uint8_t hop = 0;
	int rc;

	LOG_INF("Step %zu of %zu:", index + 1U, ARRAY_SIZE(steps));
	log_step(index);
	rc = configure(step, step->frequency_hz);
	if (rc == 0) {
		rc = rzi_lora_start(step->modulation);
	}
	if (rc != 0) {
		LOG_ERR("Step %zu setup failed: %d", index + 1U, rc);
		return rc;
	}
	if (step->hop_channels != 0U) {
		shuffle_hops(step->hop_channels);
	}
	while (duration_ms == 0U || k_uptime_get() - start < duration_ms) {
		if (step->hop_channels != 0U) {
			rc = configure(step, step->frequency_hz + hop_order[hop] * FCC_HOP_STEP_HZ);
			if (rc != 0) {
				return rc;
			}
			if (++hop == step->hop_channels) {
				hop = 0U;
				shuffle_hops(step->hop_channels);
			}
		}
		rc = send_packet((*counter)++, step->payload_size);
		if (rc != 0) {
			return rc;
		}
		++sent;
		if (k_uptime_get() >= next_log) {
			LOG_INF("Step %zu: %u packets", index + 1U, sent);
			next_log += PROGRESS_LOG_MS;
		}
	}
	LOG_INF("Step %zu done: %u packets", index + 1U, sent);
	return 0;
}

int main(void)
{
	uint32_t counter = 0;
	int rc;

	memcpy(payload, HEADER, HEADER_SIZE);
	for (size_t i = HEADER_SIZE; i < sizeof(payload); ++i) {
		payload[i] = (uint8_t)i;
	}
	LOG_INF("RZI RF certification: %s, %zu steps", REGION_NAME, ARRAY_SIZE(steps));
	for (size_t i = 0; i < ARRAY_SIZE(steps); ++i) {
		log_step(i);
	}
	rc = start_radio();
	if (rc != 0) {
		return rc;
	}
	if (CONFIG_APP_RF_CERT_ONLY_STEP != 0) {
		return run_step(CONFIG_APP_RF_CERT_ONLY_STEP - 1, 0U, &counter);
	}
	for (size_t i = 0; i < ARRAY_SIZE(steps); ++i) {
		rc = run_step(i, CONFIG_APP_RF_CERT_STEP_S * MSEC_PER_SEC, &counter);
		if (rc != 0) {
			return rc;
		}
	}
	LOG_INF("Sequence complete: %u packets", counter);
	return 0;
}
