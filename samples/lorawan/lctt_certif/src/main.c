/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <rzi/lorawan/certification.h>
#include <rzi/lorawan/lorawan.h>

LOG_MODULE_REGISTER(rzi_lctt_sample, LOG_LEVEL_INF);

#define USER_NODE         DT_PATH(zephyr_user)
#define REGION_ENUM(name) DT_CAT(RZI_LORAWAN_REGION_, name)

static K_SEM_DEFINE(stack_ready, 0, 1);

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
		LOG_INF("Joined");
		break;
	case RZI_LORAWAN_EVENT_JOIN_FAILED:
		LOG_WRN("Join failed: %d", event->error);
		break;
	case RZI_LORAWAN_EVENT_TX_DONE:
		LOG_INF("Uplink complete: %d", event->tx.status);
		break;
	case RZI_LORAWAN_EVENT_DOWNLINK:
		LOG_INF("Downlink: port %u, %u bytes, RSSI %d dBm, SNR %d/4 dB",
			event->downlink.port, (unsigned int)event->downlink.size,
			event->downlink.rssi_dbm, event->downlink.snr_quarter_db);
		break;
	case RZI_LORAWAN_EVENT_CLASS_B:
		LOG_INF("Class B state: %d", event->class_b);
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

int main(void)
{
	const bool wanted = IS_ENABLED(CONFIG_APP_LCTT_CERTIFICATION);
	rzi_lorawan_callback_handle_t handle;
	bool enabled = false;
	int rc;

	LOG_INF("RZI LoRaWAN certification (LCTT) sample");
	rc = rzi_lorawan_register_callbacks(&callbacks, &handle);
	if (rc == 0) {
		rc = configure_credentials();
	}
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

	rc = rzi_lorawan_get_certification_mode(&enabled);
	if (rc == 0 && enabled != wanted) {
		rc = rzi_lorawan_set_certification_mode(wanted);
	}
	if (rc != 0) {
		LOG_ERR("Certification mode change failed: %d", rc);
		return rc;
	}
	if (wanted) {
		LOG_INF("Certification mode on; the stack joins and the LCTT takes over");
	} else {
		LOG_INF("Certification mode off; flash another sample now");
	}
	return 0;
}
