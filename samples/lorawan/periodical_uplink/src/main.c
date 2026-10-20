/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <rzi/err.h>
#include <rzi/init.h>
#include <rzi/lorawan/lorawan.h>
#include <rzi/power/power.h>

static __maybe_unused void ignore_result(int result)
{
	ARG_UNUSED(result);
}

LOG_MODULE_REGISTER(rzi_periodical_uplink, LOG_LEVEL_INF);

#define USER_NODE          DT_PATH(zephyr_user)
#define LORAWAN_REGION     RZI_LORAWAN_REGION_EU_868
/* RZI_LORAWAN_CLASS_A, RZI_LORAWAN_CLASS_B, or RZI_LORAWAN_CLASS_C. */
#define LORAWAN_CLASS      RZI_LORAWAN_CLASS_A
#define LORAWAN_ADR        true
#define LORAWAN_DATA_RATE  RZI_LORAWAN_DR_5
#define RETRY_DELAY_MS     5000
#define UPLINK_INTERVAL_MS 60000
#define UPLINK_PORT        101

static K_SEM_DEFINE(stack_ready, 0, 1);
static K_SEM_DEFINE(join_done, 0, 1);
static volatile bool joined;

static void on_event(const struct rzi_lorawan_event *event, void *user_data)
{
	int rc;

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
		/* The modem accepts a class or data-rate change only after join.
		 * A fixed data rate disables ADR.
		 */
		rc = rzi_lorawan_set_class(LORAWAN_CLASS);
		if (rc != 0) {
			LOG_ERR("Class failed: %s", rzi_err_str(rc));
		}
		if (LORAWAN_ADR) {
			rc = rzi_lorawan_set_adr(true);
			if (rc != 0) {
				LOG_ERR("ADR failed: %s", rzi_err_str(rc));
			}
		} else {
			rc = rzi_lorawan_set_data_rate(LORAWAN_DATA_RATE);
			if (rc != 0) {
				LOG_ERR("Data rate failed: %s", rzi_err_str(rc));
			}
		}
		joined = true;
		k_sem_give(&join_done);
		break;
	case RZI_LORAWAN_EVENT_JOIN_FAILED:
		joined = false;
		k_sem_give(&join_done);
		break;
	case RZI_LORAWAN_EVENT_TXDONE:
		if (event->tx.error != 0) {
			LOG_WRN("TXDONE: %s", rzi_err_str(event->tx.error));
		} else {
			LOG_INF("TXDONE");
		}
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
		LOG_WRN("Join failed, retrying in %d s", RETRY_DELAY_MS / 1000);
		return -RZI_ERR_NOT_READY;
	}
	return 0;
}

static int configure_credentials(void)
{
	static const uint8_t dev_eui[] = DT_PROP(USER_NODE, user_lorawan_device_eui);
	static const uint8_t join_eui[] = DT_PROP(USER_NODE, user_lorawan_join_eui);
	static const uint8_t app_key[] = DT_PROP(USER_NODE, user_lorawan_app_key);
	static const uint8_t gen_app_key[] = DT_PROP(USER_NODE, user_lorawan_gen_app_key);
	int rc;

	rc = rzi_lorawan_set_dev_eui(dev_eui, sizeof(dev_eui));
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_set_app_eui(join_eui, sizeof(join_eui));
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_set_app_key(app_key, sizeof(app_key));
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_set_gen_app_key(gen_app_key, sizeof(gen_app_key));
	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_set_activation(RZI_LORAWAN_ACTIVATION_OTAA);
}

/* Highest byte first, so a server reads the counter as a normal integer. */
static void encode_uplink_counter(uint32_t counter, uint8_t payload[4])
{
	payload[0] = (uint8_t)(counter >> 24);
	payload[1] = (uint8_t)(counter >> 16);
	payload[2] = (uint8_t)(counter >> 8);
	payload[3] = (uint8_t)counter;
}

int main(void)
{
	uint32_t uplink_counter = 0;
	uint8_t payload[sizeof(uplink_counter)];
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
	rc = configure_credentials();
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
		ignore_result(rzi_power_sleep(RETRY_DELAY_MS));
	}
	LOG_INF("Joined");

	for (;;) {
		encode_uplink_counter(uplink_counter, payload);
		rc = rzi_lorawan_send(UPLINK_PORT, payload, sizeof(payload),
				      RZI_LORAWAN_MSG_UNCONFIRMED);
		if (rc == 0) {
			LOG_INF("Uplink %u on port %d", uplink_counter, UPLINK_PORT);
			uplink_counter++;
		} else if (rc != -RZI_ERR_BUSY) {
			LOG_WRN("Uplink rejected: %s", rzi_err_str(rc));
		}
		ignore_result(rzi_power_sleep(UPLINK_INTERVAL_MS));
	}
}
