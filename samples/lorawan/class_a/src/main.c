/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <rzi/lorawan/lorawan.h>
#include <rzi/power/power.h>

LOG_MODULE_REGISTER(rzi_lorawan_sample, LOG_LEVEL_INF);

#define USER_NODE          DT_PATH(zephyr_user)
#define REGION_ENUM(name)  DT_CAT(RZI_LORAWAN_REGION_, name)
#define RETRY_DELAY_MS     5000
#define UPLINK_INTERVAL_MS 60000

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

static void on_event(const struct rzi_lorawan_event *event, void *user_data)
{
	ARG_UNUSED(user_data);
	switch (event->type) {
	case RZI_LORAWAN_EVENT_READY:
		stack_ready = true;
		break;
	case RZI_LORAWAN_EVENT_STATE_CHANGED:
		if (event->state == RZI_LORAWAN_STATE_READY) {
			stack_ready = true;
		}
		break;
	case RZI_LORAWAN_EVENT_JOINED:
		joined = true;
		join_failed = false;
		break;
	case RZI_LORAWAN_EVENT_JOIN_FAILED:
		joined = false;
		join_failed = true;
		break;
	case RZI_LORAWAN_EVENT_TX_DONE:
		LOG_INF("Uplink complete: %d", event->tx.status);
		break;
	case RZI_LORAWAN_EVENT_DOWNLINK:
		LOG_INF("Downlink: port %u, %u bytes, RSSI %d dBm, SNR %d/4 dB", event->downlink.port,
			(unsigned int)event->downlink.size, event->downlink.rssi_dbm,
			event->downlink.snr_quarter_db);
		break;
	case RZI_LORAWAN_EVENT_ERROR:
		LOG_ERR("RZI error: %d", event->error);
		break;
	default:
		break;
	}
}

static const struct rzi_lorawan_callbacks callbacks = {
	.on_event = on_event,
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
	rc = rzi_power_init();
	if (rc != 0) {
		LOG_ERR("Power init failed: %d", rc);
		return rc;
	}
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
		(void)rzi_power_sleep(RETRY_DELAY_MS);
	}
	LOG_INF("Joined");

	for (;;) {
		rc = rzi_lorawan_send(1, payload, sizeof(payload) - 1, RZI_LORAWAN_MSG_UNCONFIRMED);
		if (rc != 0 && rc != -EBUSY) {
			LOG_WRN("Uplink rejected: %d", rc);
		}
		(void)rzi_power_sleep(UPLINK_INTERVAL_MS);
	}
}
