/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <rzi/lora/lora.h>
#include <rzi/lorawan/lorawan.h>

LOG_MODULE_REGISTER(rzi_tx_cw, LOG_LEVEL_INF);

#define READY_TIMEOUT K_SECONDS(10)

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
	int rc = rzi_lorawan_register_callbacks(&callbacks);

	if (rc != 0) {
		LOG_ERR("Callback registration failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_init(RZI_LORAWAN_REGION_EU_868);
	if (rc != 0) {
		LOG_ERR("LoRaWAN init failed: %s", rzi_err_str(rc));
		return rc;
	}
	if (k_sem_take(&stack_ready, READY_TIMEOUT) != 0) {
		LOG_ERR("LoRaWAN ready timed out");
		return -RZI_ERR_TIMEOUT;
	}
	rc = rzi_lora_start(RZI_LORA_MOD_LORA);
	if (rc != 0) {
		LOG_ERR("LoRa start failed: %s", rzi_err_str(rc));
	}
	return rc;
}

int main(void)
{
	const uint32_t duration_ms = CONFIG_APP_TX_CW_DURATION_S * MSEC_PER_SEC;
	int rc;

	LOG_INF("RZI continuous-wave test: %d Hz, %d dBm", CONFIG_APP_TX_CW_FREQUENCY_HZ,
		CONFIG_APP_TX_CW_POWER_DBM);
	rc = start_radio();
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lora_test_cw(CONFIG_APP_TX_CW_FREQUENCY_HZ, CONFIG_APP_TX_CW_POWER_DBM,
			      duration_ms);
	if (rc != 0) {
		LOG_ERR("Carrier start failed: %s", rzi_err_str(rc));
		return rc;
	}
	if (duration_ms == 0U) {
		LOG_INF("Carrier on until reset");
		return 0;
	}
	LOG_INF("Carrier on for %d s", CONFIG_APP_TX_CW_DURATION_S);
	k_sleep(K_MSEC(duration_ms));
	rc = rzi_lora_test_stop();
	if (rc != 0) {
		LOG_ERR("Carrier stop failed: %s", rzi_err_str(rc));
		return rc;
	}
	LOG_INF("Carrier off");
	return 0;
}
