/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <rzi/lorawan/fuota.h>
#include <rzi/lorawan/lorawan.h>

static __maybe_unused void ignore_result(int result)
{
	ARG_UNUSED(result);
}

LOG_MODULE_REGISTER(rzi_fuota_sample, LOG_LEVEL_INF);

#define USER_NODE       DT_PATH(zephyr_user)
#define LORAWAN_REGION  RZI_LORAWAN_REGION_EU_868
#define RETRY_DELAY_S   5
#define RETRY_DELAY     K_SECONDS(RETRY_DELAY_S)
#define UPLINK_INTERVAL K_SECONDS(5)

static K_SEM_DEFINE(stack_ready, 0, 1);
static K_SEM_DEFINE(join_done, 0, 1);
static volatile bool joined;

static const char *tx_status_str(enum rzi_lorawan_tx_status status)
{
	switch (status) {
	case RZI_LORAWAN_EVENT_TXDONE_SENT:
		return "TXDONE_SENT";
	case RZI_LORAWAN_EVENT_TXDONE_CONFIRMED:
		return "TXDONE_CONFIRMED";
	case RZI_LORAWAN_EVENT_TXDONE_NOT_SENT:
		return "TXDONE_NOT_SENT";
	default:
		return "TXDONE_UNKNOWN";
	}
}

static void log_uplink_done(const struct rzi_lorawan_tx_result *tx)
{
	if (tx->error != 0) {
		LOG_WRN("%s: %s", tx_status_str(tx->status), rzi_err_str(tx->error));
		return;
	}
	LOG_INF("%s", tx_status_str(tx->status));
}

static const char *fuota_state_str(enum rzi_fuota_state state)
{
	switch (state) {
	case RZI_FUOTA_STATE_IDLE:
		return "idle";
	case RZI_FUOTA_STATE_READY:
		return "ready";
	case RZI_FUOTA_STATE_TRANSFERRING:
		return "transferring";
	case RZI_FUOTA_STATE_COMPLETE:
		return "complete";
	case RZI_FUOTA_STATE_FAILED:
		return "failed";
	case RZI_FUOTA_STATE_APPLYING:
		return "applying";
	default:
		return "unknown";
	}
}

static void on_event(const struct rzi_lorawan_event *event, void *user_data)
{
	ARG_UNUSED(user_data);
	switch (event->type) {
	case RZI_LORAWAN_EVENT_READY:
		k_sem_give(&stack_ready);
		break;
	case RZI_LORAWAN_EVENT_STATE_CHANGED:
		if (event->state == RZI_LORAWAN_STATE_READY) {
			k_sem_give(&stack_ready);
		}
		break;
	case RZI_LORAWAN_EVENT_JOINED:
		joined = true;
		k_sem_give(&join_done);
		break;
	case RZI_LORAWAN_EVENT_JOIN_FAILED:
		joined = false;
		k_sem_give(&join_done);
		break;
	case RZI_LORAWAN_EVENT_TXDONE:
		log_uplink_done(&event->tx);
		break;
	case RZI_LORAWAN_EVENT_DOWNDATA:
		LOG_INF("Downlink: port %u, %u bytes, RSSI %d dBm, SNR %d/4 dB",
			event->downlink.port, (unsigned int)event->downlink.size,
			event->downlink.rssi_dbm, event->downlink.snr_quarter_db);
		break;
	case RZI_LORAWAN_EVENT_ERROR:
		LOG_ERR("Error: %s", rzi_err_str(event->error));
		break;
	default:
		break;
	}
}

static void on_fuota_state(enum rzi_fuota_state state, void *user_data)
{
	ARG_UNUSED(user_data);
	LOG_INF("FUOTA %s", fuota_state_str(state));
}

static void on_fuota_session_started(void *user_data)
{
	ARG_UNUSED(user_data);
	LOG_INF("FUOTA transfer started");
}

static void on_fuota_complete(int status, void *user_data)
{
	ARG_UNUSED(user_data);
	if (status != 0) {
		LOG_ERR("FUOTA failed: %s", rzi_err_str(status));
		return;
	}
	LOG_INF("FUOTA finished");
}

static const struct rzi_lorawan_callbacks callbacks = {
	.on_event = on_event,
};

static const struct rzi_fuota_callbacks fuota_callbacks = {
	.state_changed = on_fuota_state,
	.session_started = on_fuota_session_started,
	.complete = on_fuota_complete,
};

static int request_join(void)
{
	int rc;

	joined = false;
	k_sem_reset(&join_done);
	rc = rzi_lorawan_join(1, 0, RZI_LORAWAN_JOIN_INTERVAL_DEFAULT, 0);
	if (rc != 0) {
		LOG_WRN("Join rejected: %s", rzi_err_str(rc));
		return rc;
	}
	LOG_INF("Joining");
	k_sem_take(&join_done, K_FOREVER);
	if (!joined) {
		LOG_WRN("Join failed, retrying in %d s", RETRY_DELAY_S);
		return -RZI_ERR_NOT_READY;
	}
	return 0;
}

int main(void)
{
	static const uint8_t dev_eui[] = DT_PROP(USER_NODE, user_lorawan_device_eui);
	static const uint8_t join_eui[] = DT_PROP(USER_NODE, user_lorawan_join_eui);
	static const uint8_t app_key[] = DT_PROP(USER_NODE, user_lorawan_app_key);
	static const uint8_t gen_app_key[] = DT_PROP(USER_NODE, user_lorawan_gen_app_key);
	static const uint8_t payload[] = "RZI FUOTA";
	int rc;

	LOG_INF("RZI LoRaWAN FUOTA sample");
	rc = rzi_fuota_register_callbacks(&fuota_callbacks);
	if (rc != 0) {
		LOG_ERR("FUOTA callback registration failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_register_callbacks(&callbacks);
	if (rc != 0) {
		LOG_ERR("Callback registration failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_set_dev_eui(dev_eui, sizeof(dev_eui));
	if (rc != 0) {
		LOG_ERR("Credentials failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_set_app_eui(join_eui, sizeof(join_eui));
	if (rc != 0) {
		LOG_ERR("Credentials failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_set_app_key(app_key, sizeof(app_key));
	if (rc != 0) {
		LOG_ERR("Credentials failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_set_gen_app_key(gen_app_key, sizeof(gen_app_key));
	if (rc != 0) {
		LOG_ERR("Credentials failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_set_activation(RZI_LORAWAN_ACTIVATION_OTAA);
	if (rc != 0) {
		LOG_ERR("Credentials failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_init(LORAWAN_REGION);
	if (rc != 0) {
		LOG_ERR("LoRaWAN init failed: %s", rzi_err_str(rc));
		return rc;
	}

	k_sem_take(&stack_ready, K_FOREVER);

	while (request_join() != 0) {
		ignore_result(rzi_lorawan_leave());
		k_sleep(RETRY_DELAY);
	}
	LOG_INF("Joined");

	for (;;) {
		rc = rzi_lorawan_send(1, payload, sizeof(payload) - 1, RZI_LORAWAN_MSG_UNCONFIRMED);
		if (rc != 0 && rc != -RZI_ERR_BUSY) {
			LOG_WRN("Uplink rejected: %s", rzi_err_str(rc));
		}
		k_sleep(UPLINK_INTERVAL);
	}
}
