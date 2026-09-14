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

static void on_state_changed(enum rzi_lorawan_state state, void *user_data)
{
	int rc;

	ARG_UNUSED(user_data);
	if (state != RZI_LORAWAN_STATE_READY) {
		return;
	}

	rc = rzi_lorawan_join(&join_config);
	if (rc != 0) {
		LOG_WRN("Join rejected: %d", rc);
	}
}

static void on_join_done(int status, void *user_data)
{
	ARG_UNUSED(user_data);
	if (status != 0) {
		LOG_INF("Join failed: %d", status);
		return;
	}

	LOG_INF("Joined");
	send_uplink_counter();
	(void)k_work_schedule(&uplink_work, K_SECONDS(PERIODICAL_UPLINK_DELAY_S));
}

static void on_send_done(const struct rzi_lorawan_tx_result *result, void *user_data)
{
	ARG_UNUSED(user_data);
	LOG_INF("TX done %d", result->status);
}

static void on_downlink(const struct rzi_lorawan_downlink *downlink, void *user_data)
{
	ARG_UNUSED(user_data);
	LOG_INF("RX port %u, %u bytes, RSSI %d", downlink->port, (unsigned int)downlink->size,
		downlink->rssi_dbm);
}

static void on_error(int error, void *user_data)
{
	ARG_UNUSED(user_data);
	LOG_ERR("RZI error %d", error);
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
