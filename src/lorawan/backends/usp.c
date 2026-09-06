// SPDX-License-Identifier: Apache-2.0
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/lorawan_lbm/lorawan_hal_init.h>
#include <zephyr/usp/smtc_sw_platform_helper.h>
#include <smtc_modem_api.h>
#include <smtc_modem_utilities.h>
#include <smtc_modem_hal.h>
#include <rzi/lorawan.h>

#include "../backend.h"

#define STACK_ID 0
/* usp_zephyr currently leaves registration to its integrating application. */
LOG_MODULE_REGISTER(usp, CONFIG_USP_LOG_LEVEL);
BUILD_ASSERT(CONFIG_USP_THREADS_MUTEXES, "RZI needs USP serialization");
BUILD_ASSERT(RZI_LORAWAN_MAX_PAYLOAD == SMTC_MODEM_MAX_LORAWAN_PAYLOAD_LENGTH);

/* Protected by rac_api_mutex, also held by the USP engine callback. */
static bool ready;
static bool tx_pending;
static struct rzi_lorawan_config settings;
static smtc_modem_region_t region;
static rzi_lorawan_event_sink_t event_sink;

static int result(smtc_modem_return_code_t rc)
{
	switch (rc) {
	case SMTC_MODEM_RC_OK:
		return 0;
	case SMTC_MODEM_RC_NOT_INIT:
		return -EAGAIN;
	case SMTC_MODEM_RC_INVALID:
	case SMTC_MODEM_RC_INVALID_STACK_ID:
		return -EINVAL;
	case SMTC_MODEM_RC_BUSY:
		return -EBUSY;
	case SMTC_MODEM_RC_NO_TIME:
	case SMTC_MODEM_RC_NO_EVENT:
		return -EAGAIN;
	default:
		return -EIO;
	}
}

static const smtc_modem_region_t regions[] = {
	[RZI_LORAWAN_REGION_EU_868] = SMTC_MODEM_REGION_EU_868,
	[RZI_LORAWAN_REGION_US_915] = SMTC_MODEM_REGION_US_915,
	[RZI_LORAWAN_REGION_AU_915] = SMTC_MODEM_REGION_AU_915,
	[RZI_LORAWAN_REGION_CN_470] = SMTC_MODEM_REGION_CN_470,
	[RZI_LORAWAN_REGION_AS_923_GRP1] = SMTC_MODEM_REGION_AS_923_GRP1,
	[RZI_LORAWAN_REGION_AS_923_GRP2] = SMTC_MODEM_REGION_AS_923_GRP2,
	[RZI_LORAWAN_REGION_AS_923_GRP3] = SMTC_MODEM_REGION_AS_923_GRP3,
	[RZI_LORAWAN_REGION_AS_923_GRP4] = SMTC_MODEM_REGION_AS_923_GRP4,
	[RZI_LORAWAN_REGION_IN_865] = SMTC_MODEM_REGION_IN_865,
	[RZI_LORAWAN_REGION_KR_920] = SMTC_MODEM_REGION_KR_920,
	[RZI_LORAWAN_REGION_RU_864] = SMTC_MODEM_REGION_RU_864,
};

static int map_region(enum rzi_lorawan_region value, smtc_modem_region_t *out)
{
	if ((unsigned int)value >= ARRAY_SIZE(regions)) {
		return -EINVAL;
	}

	*out = regions[value];
	return 0;
}

static void publish(const struct rzi_lorawan_event *event)
{
	event_sink(event);
}

static void publish_error(int error)
{
	struct rzi_lorawan_event event = {.type = RZI_LORAWAN_ERROR, .error = error};

	publish(&event);
}

static int configure(void)
{
	int rc;

	rc = result(smtc_modem_set_deveui(STACK_ID, settings.dev_eui));
	if (rc != 0) {
		return rc;
	}
	rc = result(smtc_modem_set_joineui(STACK_ID, settings.join_eui));
	if (rc != 0) {
		return rc;
	}
	rc = result(smtc_modem_set_appkey(STACK_ID, settings.application_key));
	if (rc != 0) {
		return rc;
	}
	rc = result(smtc_modem_set_nwkkey(STACK_ID, settings.network_key));
	if (rc != 0) {
		return rc;
	}
	rc = result(smtc_modem_set_region(STACK_ID, region));
	if (rc != 0) {
		return rc;
	}
	rc = result(smtc_modem_set_join_duty_cycle_backoff_bypass(STACK_ID,
								  settings.join_backoff_bypass));
	if (rc != 0) {
		return rc;
	}

	return 0;
}

static void modem_event_callback(void)
{
	smtc_modem_event_t source;
	uint8_t pending = 0;

	do {
		smtc_modem_return_code_t rc = smtc_modem_get_event(&source, &pending);
		struct rzi_lorawan_event event = {0};

		if (rc == SMTC_MODEM_RC_NO_EVENT) {
			break;
		}
		if (rc != SMTC_MODEM_RC_OK) {
			publish_error(result(rc));
			break;
		}
		switch (source.event_type) {
		case SMTC_MODEM_EVENT_RESET: {
			int error;

			ready = false;
			tx_pending = false;
			error = configure();
			if (error != 0) {
				publish_error(error);
				break;
			}
			ready = true;
			event.type = RZI_LORAWAN_READY;
			publish(&event);
			break;
		}
		case SMTC_MODEM_EVENT_JOINED:
			event.type = RZI_LORAWAN_JOINED;
			publish(&event);
			break;
		case SMTC_MODEM_EVENT_JOINFAIL:
			event.type = RZI_LORAWAN_JOIN_FAILED;
			publish(&event);
			break;
		case SMTC_MODEM_EVENT_TXDONE:
			tx_pending = false;
			event.type = RZI_LORAWAN_TX_DONE;
			switch (source.event_data.txdone.status) {
			case SMTC_MODEM_EVENT_TXDONE_CONFIRMED:
				event.tx_result = RZI_LORAWAN_TX_ACKED;
				break;
			case SMTC_MODEM_EVENT_TXDONE_SENT:
				event.tx_result = RZI_LORAWAN_TX_SENT;
				break;
			default:
				event.tx_result = RZI_LORAWAN_TX_NOT_SENT;
				break;
			}
			publish(&event);
			break;
		case SMTC_MODEM_EVENT_DOWNDATA: {
			uint8_t remaining = 0;

			do {
				smtc_modem_dl_metadata_t meta = {0};

				event.type = RZI_LORAWAN_DOWNLINK;
				rc = smtc_modem_get_downlink_data(event.downlink.data,
								  &event.downlink.size, &meta,
								  &remaining);
				if (rc != SMTC_MODEM_RC_OK) {
					publish_error(result(rc));
					break;
				}
				event.downlink.port = meta.fport;
				event.downlink.rssi_dbm = (int16_t)meta.rssi - 64;
				event.downlink.snr_quarter_db = meta.snr;
				publish(&event);
			} while (remaining > 0);
			break;
		}
		default:
			break;
		}
	} while (pending > 0);
}

static int usp_init(const struct rzi_lorawan_config *config, rzi_lorawan_event_sink_t sink)
{
	smtc_modem_region_t selected;

	if (map_region(config->region, &selected) != 0) {
		return -EINVAL;
	}
	settings = *config;
	region = selected;
	event_sink = sink;
	SMTC_SW_PLATFORM_INIT();
	k_mutex_lock(&rac_api_mutex, K_FOREVER);
	smtc_rac_init();
	smtc_modem_init(modem_event_callback);
	k_mutex_unlock(&rac_api_mutex);
	smtc_modem_hal_wake_up();
	return 0;
}

static int enter(void)
{
	k_mutex_lock(&rac_api_mutex, K_FOREVER);
	if (!ready) {
		k_mutex_unlock(&rac_api_mutex);
		return -EAGAIN;
	}
	return 0;
}

static int finish(int rc)
{
	k_mutex_unlock(&rac_api_mutex);
	smtc_modem_hal_wake_up();
	return rc;
}

static int usp_join(void)
{
	int rc = enter();

	if (rc != 0) {
		return rc;
	}
	return finish(result(smtc_modem_join_network(STACK_ID)));
}

static int usp_leave(void)
{
	int rc = enter();

	if (rc != 0) {
		return rc;
	}
	/* Preserve completion ownership: do not cancel an outstanding TX. */
	if (tx_pending) {
		return finish(-EBUSY);
	}
	return finish(result(smtc_modem_leave_network(STACK_ID)));
}

static int usp_send(uint8_t port, const uint8_t *data, size_t size, bool confirmed)
{
	int rc = enter();

	if (rc != 0) {
		return rc;
	}
	if (tx_pending) {
		return finish(-EBUSY);
	}
	rc = result(smtc_modem_request_uplink(STACK_ID, port, confirmed, data, size));
	if (rc == 0) {
		tx_pending = true;
	}
	return finish(rc);
}

static int usp_is_joined(bool *joined)
{
	int rc = enter();
	smtc_modem_status_mask_t status;

	if (rc != 0) {
		return rc;
	}
	rc = result(smtc_modem_get_status(STACK_ID, &status));
	if (rc == 0) {
		*joined = (status & SMTC_MODEM_STATUS_JOINED) != 0;
	}
	return finish(rc);
}

const struct rzi_lorawan_backend_api rzi_lorawan_backend = {
	.init = usp_init,
	.join = usp_join,
	.leave = usp_leave,
	.send = usp_send,
	.is_joined = usp_is_joined,
};
