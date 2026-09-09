/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <rzi/lorawan.h>

LOG_MODULE_REGISTER(rzi_lorawan_sample, LOG_LEVEL_INF);

#define USER_NODE         DT_PATH(zephyr_user)
#define REGION_ENUM(name) DT_CAT(RZI_LORAWAN_REGION_, name)
#define RETRY_DELAY       K_SECONDS(5)
#define UPLINK_INTERVAL   K_SECONDS(60)

static volatile bool stack_ready;
static volatile bool joined;
static volatile bool join_failed;

static const struct rzi_lorawan_join_config join_config = {
	.activation = RZI_LORAWAN_ACTIVATION_OTAA,
	.otaa.dev_eui = DT_PROP(USER_NODE, user_lorawan_device_eui),
	.otaa.join_eui = DT_PROP(USER_NODE, user_lorawan_join_eui),
	.otaa.network_key = DT_PROP(USER_NODE, user_lorawan_app_key),
	.otaa.application_key = DT_PROP(USER_NODE, user_lorawan_gen_app_key),
};

static void wait_until(volatile bool *flag)
{
	while (!*flag) {
		k_sleep(K_MSEC(10));
	}
}

static void on_state_changed(enum rzi_lorawan_state state, void *user_data)
{
	ARG_UNUSED(user_data);
	if (state == RZI_LORAWAN_STATE_READY) {
		stack_ready = true;
	}
}

static void on_join_done(int status, void *user_data)
{
	ARG_UNUSED(user_data);
	joined = status == 0;
	join_failed = status != 0;
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

static int request_join(void)
{
	int rc;

	joined = false;
	join_failed = false;
	rc = rzi_lorawan_join(&join_config);
	if (rc != 0) {
		LOG_WRN("Join request rejected: %d", rc);
		return rc;
	}
	while (!joined && !join_failed) {
		k_sleep(K_MSEC(10));
	}
	return joined ? 0 : -EAGAIN;
}

int main(void)
{
	static const uint8_t payload[] = "Hello from RZI";
	rzi_lorawan_callback_handle_t handle;
	int rc;

	LOG_INF("RZI LoRaWAN Class A sample");
	rc = rzi_lorawan_register_callbacks(&callbacks, &handle);
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

	wait_until(&stack_ready);

	while (request_join() != 0) {
		(void)rzi_lorawan_leave();
		k_sleep(RETRY_DELAY);
	}
	LOG_INF("Joined");

	for (;;) {
		rc = rzi_lorawan_send(1, payload, sizeof(payload) - 1, RZI_LORAWAN_MSG_UNCONFIRMED);
		if (rc != 0 && rc != -EBUSY) {
			LOG_WRN("Uplink rejected: %d", rc);
		}
		k_sleep(UPLINK_INTERVAL);
	}
}
