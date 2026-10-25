/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <rzi/err.h>
#include <rzi/init.h>
#include <rzi/lorawan/lorawan.h>

LOG_MODULE_REGISTER(rzi_periodical_uplink, LOG_LEVEL_INF);

#define USER_NODE           DT_PATH(zephyr_user)
#define LORAWAN_REGION      RZI_LORAWAN_REGION_EU_868
/* RZI_LORAWAN_CLASS_A, RZI_LORAWAN_CLASS_B, or RZI_LORAWAN_CLASS_C. */
#define LORAWAN_CLASS       RZI_LORAWAN_CLASS_A
#define LORAWAN_ADR         true
#define LORAWAN_DATA_RATE   RZI_LORAWAN_DR_5
#define RETRY_DELAY_MS      5000
#define UPLINK_INTERVAL_MS  60000
#define UPLINK_PORT         101
/* Extra wait-time diagnostics only for EU868; set to 0 to disable. */
#define LOG_DUTY_CYCLE_WAIT (LORAWAN_REGION == RZI_LORAWAN_REGION_EU_868)

static uint32_t uplink_counter;

static void join_work_handler(struct k_work *work);
static void uplink_work_handler(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(join_work, join_work_handler);
static K_WORK_DELAYABLE_DEFINE(uplink_work, uplink_work_handler);

static void on_event(const struct rzi_lorawan_event *event, void *user_data)
{
	uint32_t backoff_s;
	int rc;

	ARG_UNUSED(user_data);
	switch (event->type) {
	case RZI_LORAWAN_EVENT_READY:
		/* RZI publishes READY after LBM RESET setup has completed. */
		k_work_reschedule(&join_work, K_NO_WAIT);
		break;
	case RZI_LORAWAN_EVENT_JOINED:
		LOG_INF("Joined");

		/* Class and data-rate settings are accepted only after joining. */
		rc = rzi_lorawan_set_class(LORAWAN_CLASS);
		if (rc != 0) {
			LOG_ERR("Class configuration failed: %s", rzi_err_str(rc));
			return;
		}
		if (LORAWAN_ADR) {
			rc = rzi_lorawan_set_adr(true);
		} else {
			/* Selecting a fixed data rate also disables ADR. */
			rc = rzi_lorawan_set_data_rate(LORAWAN_DATA_RATE);
		}
		if (rc != 0) {
			LOG_ERR("Data rate/ADR configuration failed: %s", rzi_err_str(rc));
			return;
		}

		k_work_reschedule(&uplink_work, K_NO_WAIT);
		break;
	case RZI_LORAWAN_EVENT_JOIN_FAILED:
		LOG_WRN("Join attempt failed; retry scheduled");
		rc = rzi_lorawan_get_join_backoff(&backoff_s);
		if (rc == 0 && backoff_s > 0U) {
			LOG_INF("Join backoff remaining: %u s", backoff_s);
		} else if (rc != 0 && rc != -RZI_ERR_NOT_SUPPORTED) {
			LOG_WRN("Join backoff query failed: %s", rzi_err_str(rc));
		}
		k_work_reschedule(&join_work, K_MSEC(RETRY_DELAY_MS));
		break;
	case RZI_LORAWAN_EVENT_TXDONE:
		if (event->tx.error != 0) {
			LOG_WRN("TXDONE: %s", rzi_err_str(event->tx.error));
		} else {
			LOG_INF("TXDONE");
		}
		/* Start the interval after the complete Class A exchange. */
		k_work_reschedule(&uplink_work, K_MSEC(UPLINK_INTERVAL_MS));
		break;
	case RZI_LORAWAN_EVENT_DOWNDATA:
		LOG_INF("DOWNDATA: port %u, %u bytes, RSSI %d dBm, SNR %d/4 dB",
			event->downlink.port, (unsigned int)event->downlink.size,
			event->downlink.rssi_dbm, event->downlink.snr_quarter_db);
		break;
	case RZI_LORAWAN_EVENT_ERROR:
		LOG_ERR("Error: %s", rzi_err_str(event->error));
		break;
	case RZI_LORAWAN_EVENT_LINK_CHECK:
		LOG_INF("LINK_CHECK: margin %u dB, %u gateways, RSSI %d dBm, SNR %d/4 dB",
			event->link_check.demod_margin, event->link_check.gateway_count,
			event->link_check.rssi_dbm, event->link_check.snr_quarter_db);
		break;
	case RZI_LORAWAN_EVENT_LORAWAN_MAC_TIME:
		if (event->error != 0) {
			LOG_WRN("LORAWAN_MAC_TIME: %s", rzi_err_str(event->error));
		} else {
			LOG_INF("LORAWAN_MAC_TIME");
		}
		break;
	case RZI_LORAWAN_EVENT_CLASS_B:
		switch (event->class_b) {
		case RZI_LORAWAN_CLASS_B_IDLE:
			LOG_INF("CLASS_B: IDLE");
			break;
		case RZI_LORAWAN_CLASS_B_ACQUIRING_BEACON:
			LOG_INF("CLASS_B: ACQUIRING_BEACON");
			break;
		case RZI_LORAWAN_CLASS_B_ACQUIRING_PING_SLOT:
			LOG_INF("CLASS_B: ACQUIRING_PING_SLOT");
			break;
		case RZI_LORAWAN_CLASS_B_SWITCHING:
			LOG_INF("CLASS_B: SWITCHING");
			break;
		case RZI_LORAWAN_CLASS_B_ACTIVE:
			LOG_INF("CLASS_B: ACTIVE");
			break;
		default:
			break;
		}
		break;
	case RZI_LORAWAN_EVENT_REGIONAL_DUTY_CYCLE:
		LOG_INF("REGIONAL_DUTY_CYCLE: %s", event->duty_cycle_limited ? "limited" : "clear");
		if (LOG_DUTY_CYCLE_WAIT && event->duty_cycle_limited) {
			int32_t time_ms;

			rc = rzi_lorawan_get_duty_cycle_status(&time_ms);
			if (rc == 0 && time_ms < 0) {
				LOG_INF("Duty-cycle wait remaining: %lld ms", -(long long)time_ms);
			} else if (rc != 0 && rc != -RZI_ERR_NOT_SUPPORTED) {
				LOG_WRN("Duty-cycle query failed: %s", rzi_err_str(rc));
			}
		}
		break;
	case RZI_LORAWAN_EVENT_NO_DOWNLINK_THRESHOLD:
		if ((event->no_downlink & RZI_LORAWAN_EVENT_NO_RX_THRESHOLD_ADR_BACKOFF_END) !=
		    0U) {
			LOG_WRN("NO_DOWNLINK_THRESHOLD: ADR backoff ended");
		}
		if ((event->no_downlink & RZI_LORAWAN_EVENT_NO_RX_THRESHOLD_USER_THRESHOLD) != 0U) {
			LOG_WRN("NO_DOWNLINK_THRESHOLD: user threshold reached");
		}
		break;
	default:
		break;
	}
}

static const struct rzi_lorawan_callbacks callbacks = {
	.on_event = on_event,
};

static void join_work_handler(struct k_work *work)
{
	int rc;

	ARG_UNUSED(work);

	/* One attempt; JOIN_FAILED schedules the next request. */
	rc = rzi_lorawan_join(1, 0, RZI_LORAWAN_JOIN_INTERVAL_DEFAULT, 0);
	if (rc != 0) {
		LOG_WRN("Join request rejected: %s; retry scheduled", rzi_err_str(rc));
		k_work_reschedule(&join_work, K_MSEC(RETRY_DELAY_MS));
		return;
	}

	LOG_INF("Join request accepted; waiting for Join-Accept");
}

static void uplink_work_handler(struct k_work *work)
{
	uint8_t payload[sizeof(uplink_counter)];
	int rc;

	ARG_UNUSED(work);

	/* Network byte order: the server receives 00000000, 00000001, ... */
	payload[0] = (uint8_t)(uplink_counter >> 24);
	payload[1] = (uint8_t)(uplink_counter >> 16);
	payload[2] = (uint8_t)(uplink_counter >> 8);
	payload[3] = (uint8_t)uplink_counter;
	rc = rzi_lorawan_send(UPLINK_PORT, payload, sizeof(payload), RZI_LORAWAN_MSG_UNCONFIRMED);
	if (rc != 0) {
		LOG_WRN("Uplink request rejected: %s; retry scheduled", rzi_err_str(rc));
		k_work_reschedule(&uplink_work, K_MSEC(RETRY_DELAY_MS));
		return;
	}

	LOG_INF("Uplink %u accepted on port %d", uplink_counter, UPLINK_PORT);
	uplink_counter++;
	/* TXDONE schedules the next uplink; acceptance is not completion. */
}

int main(void)
{
	static const uint8_t dev_eui[] = DT_PROP(USER_NODE, user_lorawan_device_eui);
	static const uint8_t join_eui[] = DT_PROP(USER_NODE, user_lorawan_join_eui);
	static const uint8_t app_key[] = DT_PROP(USER_NODE, user_lorawan_app_key);
	static const uint8_t gen_app_key[] = DT_PROP(USER_NODE, user_lorawan_gen_app_key);
	int rc;

	LOG_INF("RZI LoRaWAN periodical uplink (%d s) sample", UPLINK_INTERVAL_MS / 1000);
	rc = rzi_init();
	if (rc != 0) {
		LOG_ERR("Init failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_register_callbacks(&callbacks);
	if (rc != 0) {
		LOG_ERR("Callback registration failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_set_dev_eui(dev_eui, sizeof(dev_eui));
	if (rc != 0) {
		LOG_ERR("Device EUI configuration failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_set_app_eui(join_eui, sizeof(join_eui));
	if (rc != 0) {
		LOG_ERR("Join EUI configuration failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_set_app_key(app_key, sizeof(app_key));
	if (rc != 0) {
		LOG_ERR("AppKey configuration failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_set_gen_app_key(gen_app_key, sizeof(gen_app_key));
	if (rc != 0) {
		LOG_ERR("GenAppKey configuration failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_set_activation(RZI_LORAWAN_ACTIVATION_OTAA);
	if (rc != 0) {
		LOG_ERR("OTAA activation configuration failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_init(LORAWAN_REGION);
	if (rc != 0) {
		LOG_ERR("LoRaWAN init failed: %s", rzi_err_str(rc));
		return rc;
	}

	/* READY starts joining; JOINED applies session settings and starts uplinks. */
	return 0;
}
