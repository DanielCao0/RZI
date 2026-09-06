/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <rzi/lorawan.h>

LOG_MODULE_REGISTER(rzi_lorawan_sample, LOG_LEVEL_INF);

#define USER_NODE          DT_PATH(zephyr_user)
#define REGION_ENUM(name)  DT_CAT(RZI_LORAWAN_REGION_, name)
#define RETRY_DELAY        K_SECONDS(5)
#define UPLINK_INTERVAL_MS 60000

static const struct rzi_lorawan_config config = {
	.region = REGION_ENUM(DT_STRING_UNQUOTED(USER_NODE, user_lorawan_region)),
	.dev_eui = DT_PROP(USER_NODE, user_lorawan_device_eui),
	.join_eui = DT_PROP(USER_NODE, user_lorawan_join_eui),
	.network_key = DT_PROP(USER_NODE, user_lorawan_app_key),
	.application_key = DT_PROP(USER_NODE, user_lorawan_gen_app_key),
};

int main(void)
{
	static const uint8_t payload[] = "Hello from RZI";
	bool joined = false;
	int rc;

	LOG_INF("RZI LoRaWAN Class A sample");
	rc = rzi_lorawan_init(&config);
	if (rc != 0) {
		LOG_ERR("Initialization failed: %d", rc);
		return rc;
	}

	for (;;) {
		struct rzi_lorawan_event event;
		int32_t timeout = joined ? UPLINK_INTERVAL_MS : -1;

		rc = rzi_lorawan_get_event(&event, timeout);
		if (rc == -EAGAIN && joined) {
			rc = rzi_lorawan_send(1, payload, sizeof(payload) - 1, false);
			if (rc != 0 && rc != -EBUSY) {
				LOG_WRN("Uplink rejected: %d", rc);
			}
			continue;
		}
		if (rc != 0) {
			LOG_ERR("Event handling failed: %d", rc);
			return rc;
		}

		switch (event.type) {
		case RZI_LORAWAN_READY:
			rc = rzi_lorawan_join();
			if (rc != 0) {
				LOG_WRN("Join request rejected: %d", rc);
			}
			break;
		case RZI_LORAWAN_JOINED:
			joined = true;
			LOG_INF("Joined");
			break;
		case RZI_LORAWAN_JOIN_FAILED:
			joined = false;
			LOG_WRN("Join failed; retrying");
			k_sleep(RETRY_DELAY);
			(void)rzi_lorawan_leave();
			(void)rzi_lorawan_join();
			break;
		case RZI_LORAWAN_TX_DONE:
			LOG_INF("Uplink complete: %d", event.tx_result);
			break;
		case RZI_LORAWAN_DOWNLINK:
			LOG_INF("Downlink: port %u, %u bytes, RSSI %d dBm, SNR %d/4 dB",
				event.downlink.port, event.downlink.size, event.downlink.rssi_dbm,
				event.downlink.snr_quarter_db);
			break;
		case RZI_LORAWAN_ERROR:
			LOG_ERR("RZI error: %d", event.error);
			return event.error;
		}
	}
}
