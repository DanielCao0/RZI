/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Periodical Class A uplink, same shape as Semtech USP periodical_uplink
 * (CONFIG_USP_MAIN_THREAD=y) and RAK RUI3-Best-Practice:
 *
 *   READY  -> join
 *   JOINED -> first uplink, start period timer
 *   timer  -> uplink, restart timer
 *   main   -> sleep forever
 *
 * The application does not compute RX1/RX2. USP's engine thread already
 * runs smtc_modem_run_engine() and sleeps for the returned sleep_time_ms.
 */

#include <errno.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <rzi/lorawan/lorawan.h>
#include <rzi/power/power.h>

LOG_MODULE_REGISTER(rzi_low_power, LOG_LEVEL_INF);

#define USER_NODE                 DT_PATH(zephyr_user)
#define REGION_ENUM(name)         DT_CAT(RZI_LORAWAN_REGION_, name)
#define PERIODICAL_UPLINK_DELAY_S 60
#define UPLINK_PORT               101

static const struct rzi_lorawan_join_config join_config = {
	.activation = RZI_LORAWAN_ACTIVATION_OTAA,
	.otaa.dev_eui = DT_PROP(USER_NODE, user_lorawan_device_eui),
	.otaa.join_eui = DT_PROP(USER_NODE, user_lorawan_join_eui),
	.otaa.network_key = DT_PROP(USER_NODE, user_lorawan_app_key),
	.otaa.application_key = DT_PROP(USER_NODE, user_lorawan_gen_app_key),
};

static uint32_t uplink_counter;
static void uplink_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(uplink_work, uplink_work_handler);

static void send_uplink_counter(void)
{
	uint8_t buff[4];
	int rc;

	sys_put_be32(uplink_counter, buff);
	rc = rzi_lorawan_send(UPLINK_PORT, buff, sizeof(buff), RZI_LORAWAN_MSG_UNCONFIRMED);
	if (rc == 0) {
		uplink_counter++;
	} else if (rc != -EBUSY) {
		LOG_WRN("Uplink rejected: %d", rc);
	}
}

static void uplink_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	send_uplink_counter();
	(void)k_work_schedule(&uplink_work, K_SECONDS(PERIODICAL_UPLINK_DELAY_S));
}

static void request_join(void)
{
	int rc = rzi_lorawan_join(&join_config);

	if (rc != 0) {
		LOG_WRN("Join rejected: %d", rc);
	}
}

static void on_event(const struct rzi_lorawan_event *event, void *user_data)
{
	ARG_UNUSED(user_data);
	switch (event->type) {
	case RZI_LORAWAN_EVENT_READY:
		request_join();
		break;
	case RZI_LORAWAN_EVENT_STATE_CHANGED:
		if (event->state == RZI_LORAWAN_STATE_READY) {
			request_join();
		}
		break;
	case RZI_LORAWAN_EVENT_JOINED:
		LOG_INF("Joined");
		send_uplink_counter();
		(void)k_work_schedule(&uplink_work, K_SECONDS(PERIODICAL_UPLINK_DELAY_S));
		break;
	case RZI_LORAWAN_EVENT_JOIN_FAILED:
		LOG_INF("Join failed: %d", event->error);
		request_join();
		break;
	case RZI_LORAWAN_EVENT_TX_DONE:
		LOG_INF("TX done %d", event->tx.status);
		break;
	case RZI_LORAWAN_EVENT_DOWNLINK:
		LOG_INF("RX port %u, %u bytes, RSSI %d", event->downlink.port,
			(unsigned int)event->downlink.size, event->downlink.rssi_dbm);
		break;
	case RZI_LORAWAN_EVENT_ERROR:
		LOG_ERR("RZI error %d", event->error);
		break;
	default:
		break;
	}
}

static const struct rzi_lorawan_callbacks callbacks = {
	.on_event = on_event,
};

int main(void)
{
	rzi_lorawan_callback_handle_t handle;
	int rc;

	rc = rzi_power_init();
	if (rc == 0) {
		rc = rzi_power_set_policy(RZI_POWER_POLICY_SUSPEND);
	}
	if (rc != 0) {
		LOG_ERR("Power init failed: %d", rc);
		return rc;
	}

	LOG_INF("RZI Class A periodical uplink");
	rc = rzi_lorawan_register_callbacks(&callbacks, &handle);
	if (rc != 0) {
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

	k_sleep(K_FOREVER);
	return 0;
}
