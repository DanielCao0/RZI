/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <rzi/lorawan/lorawan.h>
#include <rzi/power/power.h>

static __maybe_unused void ignore_result(int result)
{
	ARG_UNUSED(result);
}

LOG_MODULE_REGISTER(rzi_lorawan_sample, LOG_LEVEL_INF);

#define USER_NODE          DT_PATH(zephyr_user)
#define REGION_ENUM(name)  DT_CAT(RZI_LORAWAN_REGION_, name)
#define RETRY_DELAY_MS     5000
#define UPLINK_INTERVAL_MS 60000
#define UPLINK_PORT        101

static K_SEM_DEFINE(stack_ready, 0, 1);
static K_SEM_DEFINE(join_done, 0, 1);
static volatile bool joined;

static int configure_credentials(void)
{
	static const uint8_t dev_eui[] = DT_PROP(USER_NODE, user_lorawan_device_eui);
	static const uint8_t join_eui[] = DT_PROP(USER_NODE, user_lorawan_join_eui);
	static const uint8_t app_key[] = DT_PROP(USER_NODE, user_lorawan_app_key);
	static const uint8_t gen_app_key[] = DT_PROP(USER_NODE, user_lorawan_gen_app_key);
	int rc = rzi_lorawan_set_dev_eui(dev_eui, sizeof(dev_eui));

	if (rc == 0) {
		rc = rzi_lorawan_set_app_eui(join_eui, sizeof(join_eui));
	}
	if (rc == 0) {
		rc = rzi_lorawan_set_app_key(app_key, sizeof(app_key));
	}
	if (rc == 0) {
		rc = rzi_lorawan_set_gen_app_key(gen_app_key, sizeof(gen_app_key));
	}
	if (rc == 0) {
		rc = rzi_lorawan_set_activation(RZI_LORAWAN_ACTIVATION_OTAA);
	}
	return rc;
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
	case RZI_LORAWAN_EVENT_TX_DONE:
		LOG_INF("Uplink complete: %d", event->tx.status);
		break;
	case RZI_LORAWAN_EVENT_DOWNLINK:
		LOG_INF("Downlink: port %u, %u bytes, RSSI %d dBm, SNR %d/4 dB",
			event->downlink.port, (unsigned int)event->downlink.size,
			event->downlink.rssi_dbm, event->downlink.snr_quarter_db);
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
	k_sem_reset(&join_done);
	rc = rzi_lorawan_join(1, 0, RZI_LORAWAN_JOIN_INTERVAL_DEFAULT, 0);
	if (rc != 0) {
		LOG_WRN("Join request rejected: %d", rc);
		return rc;
	}
	LOG_INF("Joining");
	k_sem_take(&join_done, K_FOREVER);
	if (!joined) {
		LOG_WRN("Join failed, retrying in %d s", RETRY_DELAY_MS / 1000);
		return -RZI_ERR_NOT_READY;
	}
	return 0;
}

int main(void)
{
	uint32_t uplink_counter = 0;
	uint8_t payload[sizeof(uplink_counter)];
	rzi_lorawan_callback_handle_t handle;
	int rc;

	LOG_INF("RZI LoRaWAN periodical uplink (%d s) sample", UPLINK_INTERVAL_MS / 1000);
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
	rc = configure_credentials();
	if (rc == 0) {
		rc = rzi_lorawan_set_region(
			REGION_ENUM(DT_STRING_UNQUOTED(USER_NODE, user_lorawan_region)));
	}
	if (rc == 0) {
		rc = rzi_lorawan_start();
	}
	if (rc != 0) {
		LOG_ERR("Start failed: %d", rc);
		return rc;
	}

	k_sem_take(&stack_ready, K_FOREVER);

	while (request_join() != 0) {
		ignore_result(rzi_lorawan_leave());
		ignore_result(rzi_power_sleep(RETRY_DELAY_MS));
	}
	LOG_INF("Joined");

	for (;;) {
		sys_put_be32(uplink_counter, payload);
		rc = rzi_lorawan_send(UPLINK_PORT, payload, sizeof(payload),
				      RZI_LORAWAN_MSG_UNCONFIRMED);
		if (rc == 0) {
			LOG_INF("Uplink %u on port %d", uplink_counter, UPLINK_PORT);
			uplink_counter++;
		} else if (rc != -RZI_ERR_BUSY) {
			LOG_WRN("Uplink rejected: %d", rc);
		}
		ignore_result(rzi_power_sleep(UPLINK_INTERVAL_MS));
	}
}
