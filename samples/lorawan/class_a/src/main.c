/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <rzi/lorawan.h>

LOG_MODULE_REGISTER(rzi_lorawan_sample, LOG_LEVEL_INF);

#define USER_NODE         DT_PATH(zephyr_user)
#define REGION_ENUM(name) DT_CAT(RZI_LORAWAN_REGION_, name)
#define RETRY_DELAY       K_SECONDS(5)
#define UPLINK_INTERVAL   K_SECONDS(60)
#define EVENT_READY       BIT(0)
#define EVENT_JOINED      BIT(1)
#define EVENT_JOIN_FAILED BIT(2)

K_MSGQ_DEFINE(lorawan_events, sizeof(uint8_t), 8, 1);
static rzi_lorawan_callback_handle_t callback_handle;
static atomic_t join_in_progress;

static const struct rzi_lorawan_join_config join_config = {
	.activation = RZI_LORAWAN_ACTIVATION_OTAA,
	.otaa.dev_eui = DT_PROP(USER_NODE, user_lorawan_device_eui),
	.otaa.join_eui = DT_PROP(USER_NODE, user_lorawan_join_eui),
	.otaa.network_key = DT_PROP(USER_NODE, user_lorawan_app_key),
	.otaa.application_key = DT_PROP(USER_NODE, user_lorawan_gen_app_key),
};

static void on_state_changed(enum rzi_lorawan_state state, void *user_data)
{
	const uint8_t event = EVENT_READY;

	ARG_UNUSED(user_data);
	if (state == RZI_LORAWAN_STATE_READY && !atomic_get(&join_in_progress)) {
		(void)k_msgq_put(&lorawan_events, &event, K_NO_WAIT);
	}
}

static void on_join_done(int status, void *user_data)
{
	const uint8_t event = status == 0 ? EVENT_JOINED : EVENT_JOIN_FAILED;

	ARG_UNUSED(user_data);
	atomic_clear(&join_in_progress);
	(void)k_msgq_put(&lorawan_events, &event, K_NO_WAIT);
}

static void on_send_done(const struct rzi_lorawan_tx_result *result, void *user_data)
{
	ARG_UNUSED(user_data);
	LOG_INF("Uplink complete: %d", result->status);
}

static void on_downlink(const struct rzi_lorawan_downlink *downlink, void *user_data)
{
	ARG_UNUSED(user_data);
	LOG_INF("Downlink: port %u, %u bytes, RSSI %d dBm, SNR %d/4 dB", downlink->port,
		(unsigned int)downlink->size, downlink->rssi_dbm, downlink->snr_quarter_db);
}

static void on_error(int error, void *user_data)
{
	ARG_UNUSED(user_data);
	LOG_ERR("RZI error: %d", error);
}

static const struct rzi_lorawan_callbacks callbacks = {
	.join_done = on_join_done,
	.send_done = on_send_done,
	.downlink = on_downlink,
	.state_changed = on_state_changed,
	.error = on_error,
};

int main(void)
{
	static const uint8_t payload[] = "Hello from RZI";
	bool joined = false;
	int rc;

	LOG_INF("RZI LoRaWAN Class A sample");
	rc = rzi_lorawan_register_callbacks(&callbacks, &callback_handle);
	if (rc != 0) {
		LOG_ERR("Callback registration failed: %d", rc);
		return rc;
	}
	rc = rzi_lorawan_set_region(
		REGION_ENUM(DT_STRING_UNQUOTED(USER_NODE, user_lorawan_region)));
	if (rc == 0) {
		rc = rzi_lorawan_start();
	}
	if (rc != 0) {
		LOG_ERR("Start failed: %d", rc);
		return rc;
	}

	for (;;) {
		uint8_t events = 0;

		(void)k_msgq_get(&lorawan_events, &events, joined ? UPLINK_INTERVAL : K_FOREVER);

		if (events & EVENT_READY) {
			joined = false;
			atomic_set(&join_in_progress, 1);
			rc = rzi_lorawan_join(&join_config);
			if (rc != 0) {
				atomic_clear(&join_in_progress);
				if (rc != -EBUSY) {
					LOG_WRN("Join request rejected: %d", rc);
				}
				k_sleep(RETRY_DELAY);
				events = EVENT_READY;
				(void)k_msgq_put(&lorawan_events, &events, K_NO_WAIT);
			}
		}
		if (events & EVENT_JOINED) {
			joined = true;
			LOG_INF("Joined");
		}
		if (events & EVENT_JOIN_FAILED) {
			joined = false;
			LOG_WRN("Join failed; retrying");
			k_sleep(RETRY_DELAY);
			atomic_set(&join_in_progress, 1);
			rc = rzi_lorawan_leave();
			if (rc != 0) {
				LOG_WRN("Leave failed: %d", rc);
			}
			rc = rzi_lorawan_join(&join_config);
			if (rc != 0) {
				atomic_clear(&join_in_progress);
				LOG_WRN("Join retry rejected: %d", rc);
				k_sleep(RETRY_DELAY);
				events = EVENT_READY;
				(void)k_msgq_put(&lorawan_events, &events, K_NO_WAIT);
			}
		}
		if (events == 0U && joined) {
			rc = rzi_lorawan_send(1, payload, sizeof(payload) - 1,
					      RZI_LORAWAN_MSG_UNCONFIRMED);
			if (rc != 0 && rc != -EBUSY) {
				LOG_WRN("Uplink rejected: %d", rc);
			}
		}
	}
}
