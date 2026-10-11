/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <rzi/lora/lora.h>
#include <rzi/lorawan/lorawan.h>

LOG_MODULE_REGISTER(rzi_spectral_scan_sample, LOG_LEVEL_INF);

#define READY_TIMEOUT   K_SECONDS(10)
#define RSSI_SETTLE_MS  110
#define RSSI_POLL_MS    10
#define RSSI_TIMEOUT_MS 1000
#define BAR_FLOOR_DBM   (-130)
#define BAR_CEILING_DBM (-30)
#define BAR_WIDTH       25

static K_SEM_DEFINE(stack_ready, 0, 1);

static void on_event(const struct rzi_lorawan_event *event, void *user_data)
{
	ARG_UNUSED(user_data);
	if (event->type == RZI_LORAWAN_EVENT_READY ||
	    (event->type == RZI_LORAWAN_EVENT_STATE_CHANGED &&
	     event->state == RZI_LORAWAN_STATE_READY)) {
		k_sem_give(&stack_ready);
	}
}

static const struct rzi_lorawan_callbacks callbacks = {
	.on_event = on_event,
};

/* The USP raw LoRa backend runs on the LoRa Basics Modem test API, so the
 * modem must be started, but not joined, before rzi_lora_start().
 */
static int start_radio(void)
{
	rzi_lorawan_callback_handle_t handle;
	int rc = rzi_lorawan_register_callbacks(&callbacks, &handle);

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
	rc = rzi_lora_start(RZI_LORA_MOD_LORA);
	if (rc != 0) {
		LOG_ERR("Raw LoRa start failed: %d", rc);
	}
	return rc;
}

static int measure(uint32_t frequency_hz, int16_t *rssi_dbm)
{
	struct rzi_lora_config config;
	int64_t deadline;
	int rc = rzi_lora_get_config(&config);

	if (rc == 0) {
		config.frequency_hz = frequency_hz;
		rc = rzi_lora_set_config(&config);
	}
	if (rc == 0) {
		rc = rzi_lora_test_rssi();
	}
	if (rc != 0) {
		return rc;
	}
	k_sleep(K_MSEC(RSSI_SETTLE_MS));
	deadline = k_uptime_get() + RSSI_TIMEOUT_MS;
	for (;;) {
		rc = rzi_lora_test_get_rssi(rssi_dbm);
		if (rc != -RZI_ERR_BUSY || k_uptime_get() >= deadline) {
			return rc;
		}
		k_sleep(K_MSEC(RSSI_POLL_MS));
	}
}

static void fill_bar(char *bar, int avg_dbm)
{
	int level = CLAMP(avg_dbm, BAR_FLOOR_DBM, BAR_CEILING_DBM) - BAR_FLOOR_DBM;
	int fill = level * BAR_WIDTH / (BAR_CEILING_DBM - BAR_FLOOR_DBM);

	memset(bar, '#', fill);
	memset(bar + fill, '.', BAR_WIDTH - fill);
	bar[BAR_WIDTH] = '\0';
}

static int scan_channel(uint32_t frequency_hz)
{
	char bar[BAR_WIDTH + 1];
	int16_t min_dbm = INT16_MAX;
	int16_t max_dbm = INT16_MIN;
	int32_t sum_dbm = 0;
	int avg_dbm;

	for (int i = 0; i < CONFIG_APP_SCAN_SAMPLES; ++i) {
		int16_t rssi_dbm;
		int rc = measure(frequency_hz, &rssi_dbm);

		if (rc != 0) {
			LOG_ERR("%u Hz: measurement failed: %d", frequency_hz, rc);
			return rc;
		}
		min_dbm = MIN(min_dbm, rssi_dbm);
		max_dbm = MAX(max_dbm, rssi_dbm);
		sum_dbm += rssi_dbm;
	}
	avg_dbm = sum_dbm / CONFIG_APP_SCAN_SAMPLES;
	fill_bar(bar, avg_dbm);
	LOG_INF("%3u.%03u MHz  min %4d  avg %4d  max %4d dBm  |%s|", frequency_hz / 1000000U,
		(frequency_hz / 1000U) % 1000U, min_dbm, avg_dbm, max_dbm, bar);
	return 0;
}

int main(void)
{
	uint32_t sweep = 0;
	int rc;

	LOG_INF("RZI spectral scan: %d channels from %d Hz, %d Hz apart, %d samples each",
		CONFIG_APP_SCAN_CHANNELS, CONFIG_APP_SCAN_START_HZ, CONFIG_APP_SCAN_STEP_HZ,
		CONFIG_APP_SCAN_SAMPLES);
	rc = start_radio();
	if (rc != 0) {
		return rc;
	}
	for (;;) {
		LOG_INF("Sweep %u (bar from %d to %d dBm)", ++sweep, BAR_FLOOR_DBM,
			BAR_CEILING_DBM);
		for (int ch = 0; ch < CONFIG_APP_SCAN_CHANNELS; ++ch) {
			rc = scan_channel(CONFIG_APP_SCAN_START_HZ + ch * CONFIG_APP_SCAN_STEP_HZ);
			if (rc != 0) {
				return rc;
			}
		}
		if (CONFIG_APP_SCAN_INTERVAL_S == 0) {
			return 0;
		}
		k_sleep(K_SECONDS(CONFIG_APP_SCAN_INTERVAL_S));
	}
}
