/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Semtech USP and LoRa Basics Modem backend for RZI LoRaWAN.
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/lorawan_lbm/lorawan_hal_init.h>
#include <zephyr/usp/smtc_sw_platform_helper.h>
#include <smtc_modem_api.h>
#include <smtc_modem_utilities.h>
#include <smtc_modem_hal.h>
#include <rzi/lorawan/lorawan.h>
#include <rzi/version.h>

#include "../lorawan_backend.h"
#include "lorawan_backend_usp_priv.h"

#ifdef CONFIG_RZI_LORAWAN_FUOTA
#include "../../services/fuota/lorawan_fuota.h"
#ifdef CONFIG_LORA_BASICS_MODEM_FUOTA_V2
void lorawan_fragmentation_package_get_file_size(uint8_t stack_id, uint32_t *file_size);
#endif
#endif

#define STACK_ID RZI_LORAWAN_USP_STACK_ID

#define USP_CAPABILITIES                                                                           \
	(RZI_LORAWAN_CAP_OTAA | RZI_LORAWAN_CAP_CLASS_A | RZI_LORAWAN_CAP_CLASS_B |                \
	 RZI_LORAWAN_CAP_CLASS_C | RZI_LORAWAN_CAP_MULTICAST | RZI_LORAWAN_CAP_LINK_CHECK |        \
	 RZI_LORAWAN_CAP_INFORMATION | RZI_LORAWAN_CAP_NETWORK_MANAGEMENT |                        \
	 RZI_LORAWAN_CAP_DEVICE_TIME | RZI_LORAWAN_CAP_CERTIFICATION |                             \
	 (IS_ENABLED(CONFIG_RZI_LORAWAN_FUOTA) ? RZI_LORAWAN_CAP_FUOTA : 0U))
/* usp_zephyr currently leaves registration to its integrating application. */
LOG_MODULE_REGISTER(usp, CONFIG_USP_LOG_LEVEL);
BUILD_ASSERT(CONFIG_USP_THREADS_MUTEXES, "RZI needs USP serialization");
BUILD_ASSERT(RZI_LORAWAN_MAX_PAYLOAD == SMTC_MODEM_MAX_LORAWAN_PAYLOAD_LENGTH);

/* Protected by rac_api_mutex, also held by the USP engine callback. */
static bool ready;
static bool tx_pending;
static bool credentials_valid;
static struct rzi_lorawan_join_config join_settings;
static bool join_backoff_bypass;
static smtc_modem_region_t region;
static rzi_lorawan_event_sink_t event_sink;
static enum rzi_lorawan_class_b_state class_b_state = RZI_LORAWAN_CLASS_B_IDLE;

int rzi_lorawan_usp_result(smtc_modem_return_code_t rc)
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
	case SMTC_MODEM_RC_FAIL:
		return -EIO;
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

void rzi_lorawan_usp_publish(const struct rzi_lorawan_backend_event *event)
{
	event_sink(event);
}

static void publish_error(int error)
{
	struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_ERROR,
		.error = error,
	};

	rzi_lorawan_usp_publish(&event);
}

static int configure_credentials(void)
{
	int rc;
	const struct rzi_lorawan_join_otaa *otaa = &join_settings.otaa;

	rc = rzi_lorawan_usp_result(smtc_modem_set_deveui(STACK_ID, otaa->dev_eui));
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(smtc_modem_set_joineui(STACK_ID, otaa->join_eui));
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(smtc_modem_set_appkey(STACK_ID, otaa->application_key));
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(smtc_modem_set_nwkkey(STACK_ID, otaa->network_key));
	if (rc != 0) {
		return rc;
	}

	return 0;
}

static int configure_service(void)
{
	int rc;

	rc = rzi_lorawan_usp_result(smtc_modem_set_region(STACK_ID, region));
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(
		smtc_modem_set_join_duty_cycle_backoff_bypass(STACK_ID, join_backoff_bypass));
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
		struct rzi_lorawan_backend_event event = {0};

		if (rc == SMTC_MODEM_RC_NO_EVENT) {
			break;
		}
		if (rc != SMTC_MODEM_RC_OK) {
			publish_error(rzi_lorawan_usp_result(rc));
			break;
		}
		switch (source.event_type) {
		case SMTC_MODEM_EVENT_RESET: {
			int error;

			ready = false;
			tx_pending = false;
			error = configure_service();
			if (error == 0 && credentials_valid) {
				error = configure_credentials();
			}
			if (error != 0) {
				publish_error(error);
				break;
			}
			ready = true;
			event.type = RZI_LORAWAN_BACKEND_READY;
			rzi_lorawan_usp_publish(&event);
			break;
		}
		case SMTC_MODEM_EVENT_JOINED:
			event.type = RZI_LORAWAN_BACKEND_JOINED;
			rzi_lorawan_usp_publish(&event);
			break;
		case SMTC_MODEM_EVENT_JOINFAIL:
			event.type = RZI_LORAWAN_BACKEND_JOIN_FAILED;
			rzi_lorawan_usp_publish(&event);
			break;
		case SMTC_MODEM_EVENT_TXDONE:
			tx_pending = false;
			event.type = RZI_LORAWAN_BACKEND_TX_DONE;
			switch (source.event_data.txdone.status) {
			case SMTC_MODEM_EVENT_TXDONE_CONFIRMED:
				event.tx_status = RZI_LORAWAN_TX_ACKED;
				break;
			case SMTC_MODEM_EVENT_TXDONE_SENT:
				event.tx_status = RZI_LORAWAN_TX_SENT;
				break;
			default:
				event.tx_status = RZI_LORAWAN_TX_NOT_SENT;
				break;
			}
			rzi_lorawan_usp_publish(&event);
			break;
		case SMTC_MODEM_EVENT_DOWNDATA: {
			uint8_t remaining = 0;

			do {
				smtc_modem_dl_metadata_t meta = {0};

				event.type = RZI_LORAWAN_BACKEND_DOWNLINK;
				rc = smtc_modem_get_downlink_data(event.downlink.data,
								  &event.downlink.size, &meta,
								  &remaining);
				if (rc != SMTC_MODEM_RC_OK) {
					publish_error(rzi_lorawan_usp_result(rc));
					break;
				}
				event.downlink.port = meta.fport;
				event.downlink.rssi_dbm = (int16_t)meta.rssi - 64;
				event.downlink.snr_quarter_db = meta.snr;
				rzi_lorawan_usp_publish(&event);
			} while (remaining > 0);
			break;
		}
		case SMTC_MODEM_EVENT_LINK_CHECK:
			if (source.event_data.link_check.status ==
			    SMTC_MODEM_EVENT_MAC_REQUEST_ANSWERED) {
				uint8_t margin = 0;
				uint8_t gateways = 0;

				if (smtc_modem_get_lorawan_link_check_data(
					    STACK_ID, &margin, &gateways) == SMTC_MODEM_RC_OK) {
					event.type = RZI_LORAWAN_BACKEND_LINK_CHECK;
					event.link_check.demod_margin = margin;
					event.link_check.gateway_count = gateways;
					rzi_lorawan_usp_publish(&event);
				}
			}
			break;
		case SMTC_MODEM_EVENT_LORAWAN_MAC_TIME:
			event.type = RZI_LORAWAN_BACKEND_DEVICE_TIME;
			event.error = source.event_data.lorawan_mac_time.status ==
						      SMTC_MODEM_EVENT_MAC_REQUEST_ANSWERED
					      ? 0
					      : -ETIMEDOUT;
			rzi_lorawan_usp_publish(&event);
#ifdef CONFIG_RZI_LORAWAN_FUOTA
			if (event.error == 0) {
				event.type = RZI_LORAWAN_BACKEND_FUOTA;
				event.fuota.kind = RZI_LORAWAN_BACKEND_FUOTA_CLOCK_SYNCED;
				event.fuota.successful = true;
				rzi_lorawan_usp_publish(&event);
			}
#endif
			break;
		case SMTC_MODEM_EVENT_CLASS_B_STATUS: {
			enum rzi_lorawan_class_b_state next;

			if (source.event_data.class_b_status.status == SMTC_MODEM_EVENT_CLASS_B_READY) {
				next = RZI_LORAWAN_CLASS_B_ACTIVE;
			} else if (source.event_data.class_b_status.status ==
				   SMTC_MODEM_EVENT_CLASS_B_NOT_READY) {
				next = RZI_LORAWAN_CLASS_B_IDLE;
			} else {
				break;
			}
			if (rzi_lorawan_usp_set_class_b_state(next)) {
				rzi_lorawan_usp_publish_class_b();
			}
			break;
		}
#ifdef CONFIG_RZI_LORAWAN_FUOTA
		case SMTC_MODEM_EVENT_ALCSYNC_TIME:
			event.type = RZI_LORAWAN_BACKEND_FUOTA;
			event.fuota.kind = RZI_LORAWAN_BACKEND_FUOTA_CLOCK_SYNCED;
			event.fuota.successful = true;
			rzi_lorawan_usp_publish(&event);
			break;
		case SMTC_MODEM_EVENT_NEW_MULTICAST_SESSION_CLASS_C:
		case SMTC_MODEM_EVENT_NEW_MULTICAST_SESSION_CLASS_B:
			event.type = RZI_LORAWAN_BACKEND_FUOTA;
			event.fuota.kind = RZI_LORAWAN_BACKEND_FUOTA_SESSION_STARTED;
			event.fuota.successful = true;
			rzi_lorawan_usp_publish(&event);
			break;
		case SMTC_MODEM_EVENT_NO_MORE_MULTICAST_SESSION_CLASS_C:
		case SMTC_MODEM_EVENT_NO_MORE_MULTICAST_SESSION_CLASS_B:
			event.type = RZI_LORAWAN_BACKEND_FUOTA;
			event.fuota.kind = RZI_LORAWAN_BACKEND_FUOTA_SESSION_ENDED;
			event.fuota.successful = true;
			rzi_lorawan_usp_publish(&event);
			break;
		case SMTC_MODEM_EVENT_LORAWAN_FUOTA_DONE:
			event.type = RZI_LORAWAN_BACKEND_FUOTA;
			event.fuota.kind = RZI_LORAWAN_BACKEND_FUOTA_TRANSFER_DONE;
			event.fuota.successful = source.event_data.fuota_status.successful;
#ifdef CONFIG_LORA_BASICS_MODEM_FUOTA_V2
			if (event.fuota.successful) {
				lorawan_fragmentation_package_get_file_size(
					STACK_ID, &event.fuota.image_size);
			}
#endif
			rzi_lorawan_usp_publish(&event);
			break;
		case SMTC_MODEM_EVENT_FIRMWARE_MANAGEMENT:
			if (source.event_data.fmp.status ==
			    SMTC_MODEM_EVENT_FMP_REBOOT_IMMEDIATELY) {
				event.type = RZI_LORAWAN_BACKEND_FUOTA;
				event.fuota.kind = RZI_LORAWAN_BACKEND_FUOTA_REBOOT_REQUESTED;
				event.fuota.successful = true;
				rzi_lorawan_usp_publish(&event);
			}
			break;
#endif
		default:
			break;
		}
	} while (pending > 0);
}

int rzi_lorawan_usp_enter(void)
{
	k_mutex_lock(&rac_api_mutex, K_FOREVER);
	if (!ready) {
		k_mutex_unlock(&rac_api_mutex);
		return -EAGAIN;
	}
	return 0;
}

int rzi_lorawan_usp_finish(int rc)
{
	k_mutex_unlock(&rac_api_mutex);
	smtc_modem_hal_wake_up();
	return rc;
}

bool rzi_lorawan_usp_tx_pending(void)
{
	return tx_pending;
}

bool rzi_lorawan_usp_set_class_b_state(enum rzi_lorawan_class_b_state state)
{
	if (class_b_state == state) {
		return false;
	}
	class_b_state = state;
	return true;
}

void rzi_lorawan_usp_publish_class_b(void)
{
	struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_CLASS_B,
		.class_b = class_b_state,
	};

	if (event_sink != NULL) {
		event_sink(&event);
	}
}

enum rzi_lorawan_class_b_state rzi_lorawan_usp_class_b_state(void)
{
	return class_b_state;
}

static int usp_join(const struct rzi_lorawan_join_config *config)
{
	int rc;

	if (config->activation != RZI_LORAWAN_ACTIVATION_OTAA) {
		return -ENOTSUP;
	}
	rc = rzi_lorawan_usp_enter();
	if (rc != 0) {
		return rc;
	}
	join_settings = *config;
	credentials_valid = true;
	rc = configure_credentials();
	if (rc != 0) {
		return rzi_lorawan_usp_finish(rc);
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(smtc_modem_join_network(STACK_ID)));
}

static int usp_leave(void)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	/* Preserve completion ownership: do not cancel an outstanding TX. */
	if (tx_pending) {
		return rzi_lorawan_usp_finish(-EBUSY);
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(smtc_modem_leave_network(STACK_ID)));
}

static int usp_send(uint8_t port, const uint8_t *data, size_t size,
		    enum rzi_lorawan_message_type type)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	if (tx_pending) {
		return rzi_lorawan_usp_finish(-EBUSY);
	}
	rc = rzi_lorawan_usp_result(smtc_modem_request_uplink(
		STACK_ID, port, type == RZI_LORAWAN_MSG_CONFIRMED, data, size));
	if (rc == 0) {
		tx_pending = true;
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_is_joined(bool *joined)
{
	int rc = rzi_lorawan_usp_enter();
	smtc_modem_status_mask_t status;

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(smtc_modem_get_status(STACK_ID, &status));
	if (rc == 0) {
		*joined = (status & SMTC_MODEM_STATUS_JOINED) != 0;
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_set_class(enum rzi_lorawan_class device_class)
{
	smtc_modem_class_t mapped;
	enum rzi_lorawan_class_b_state previous;
	enum rzi_lorawan_class_b_state next;
	bool changed = false;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	switch (device_class) {
	case RZI_LORAWAN_CLASS_A:
		mapped = SMTC_MODEM_CLASS_A;
		next = RZI_LORAWAN_CLASS_B_IDLE;
		break;
	case RZI_LORAWAN_CLASS_B:
		mapped = SMTC_MODEM_CLASS_B;
		next = RZI_LORAWAN_CLASS_B_ACQUIRING_BEACON;
		break;
	case RZI_LORAWAN_CLASS_C:
		mapped = SMTC_MODEM_CLASS_C;
		next = RZI_LORAWAN_CLASS_B_IDLE;
		break;
	default:
		return rzi_lorawan_usp_finish(-EINVAL);
	}
	previous = rzi_lorawan_usp_class_b_state();
	changed = rzi_lorawan_usp_set_class_b_state(next);
	rc = rzi_lorawan_usp_result(smtc_modem_set_class(STACK_ID, mapped));
	if (rc != 0 && changed) {
		(void)rzi_lorawan_usp_set_class_b_state(previous);
		changed = false;
	}
	rc = rzi_lorawan_usp_finish(rc);
	if (changed) {
		rzi_lorawan_usp_publish_class_b();
	}
	return rc;
}

#ifdef CONFIG_RZI_LORAWAN_FUOTA
#define FMP_IMAGE_VALID 3U

static uint32_t usp_fuota_hw_version(void)
{
	return CONFIG_RZI_FUOTA_HW_VERSION;
}

static uint32_t usp_fuota_fw_version(void)
{
	return ((uint32_t)RZI_VERSION_MAJOR << 16) | ((uint32_t)RZI_VERSION_MINOR << 8) |
	       RZI_VERSION_PATCH;
}

static uint8_t usp_fuota_fw_status(void)
{
	return rzi_lorawan_fuota_has_image() ? FMP_IMAGE_VALID : 0U;
}

static uint32_t usp_fuota_next_fw_version(void)
{
	return usp_fuota_fw_version() + 1U;
}

static uint8_t usp_fuota_delete_status(uint32_t version)
{
	ARG_UNUSED(version);
	return 0U;
}

static struct lorawan_fuota_cb usp_fuota_cb = {
	.get_hw_version = usp_fuota_hw_version,
	.get_fw_version = usp_fuota_fw_version,
	.get_fw_status_available = usp_fuota_fw_status,
	.get_next_fw_version = usp_fuota_next_fw_version,
	.get_fw_delete_status = usp_fuota_delete_status,
};

static int usp_fuota_start_clock_sync(void)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(smtc_modem_start_alcsync_service(STACK_ID));
	if (rc == 0) {
		(void)smtc_modem_trig_lorawan_mac_request(STACK_ID,
							  SMTC_MODEM_LORAWAN_MAC_REQ_DEVICE_TIME);
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_fuota_get_image_size(size_t *size)
{
#ifdef CONFIG_LORA_BASICS_MODEM_FUOTA_V2
	uint32_t file_size = 0;

	if (size == NULL) {
		return -EINVAL;
	}
	lorawan_fragmentation_package_get_file_size(STACK_ID, &file_size);
	if (file_size == 0U) {
		return -ENODATA;
	}
	*size = file_size;
	return 0;
#else
	ARG_UNUSED(size);
	return -ENODATA;
#endif
}

static int usp_fuota_read_image(uint32_t offset, uint8_t *buffer, size_t size)
{
	smtc_modem_hal_context_restore(CONTEXT_FUOTA, offset, buffer, (uint32_t)size);
	return 0;
}

static int usp_fuota_reboot(void)
{
	smtc_modem_hal_reset_mcu();
	return 0;
}

static const struct rzi_lorawan_fuota_ops usp_fuota_ops = {
	.start_clock_sync = usp_fuota_start_clock_sync,
	.get_image_size = usp_fuota_get_image_size,
	.read_image = usp_fuota_read_image,
	.reboot = usp_fuota_reboot,
};

const struct rzi_lorawan_backend_extension rzi_lorawan_usp_fuota_extension = {
	.size = sizeof(usp_fuota_ops),
	.version = RZI_LORAWAN_FUOTA_OPS_VERSION,
	.api = &usp_fuota_ops,
};
#endif

static int usp_start(enum rzi_lorawan_region selected_region, bool bypass,
		     rzi_lorawan_event_sink_t sink)
{
	smtc_modem_region_t selected;

	if (map_region(selected_region, &selected) != 0 || sink == NULL) {
		return -EINVAL;
	}
	region = selected;
	join_backoff_bypass = bypass;
	event_sink = sink;
#ifdef CONFIG_RZI_LORAWAN_FUOTA
	lorawan_register_fuota_callbacks(&usp_fuota_cb);
#endif
	SMTC_SW_PLATFORM_INIT();
	k_mutex_lock(&rac_api_mutex, K_FOREVER);
	smtc_rac_init();
	smtc_modem_init(modem_event_callback);
	k_mutex_unlock(&rac_api_mutex);
	smtc_modem_hal_wake_up();
	return 0;
}

const struct rzi_lorawan_backend_api rzi_lorawan_backend = {
	.capabilities = USP_CAPABILITIES,
	.start = usp_start,
	.join = usp_join,
	.leave = usp_leave,
	.send = usp_send,
	.set_class = usp_set_class,
	.is_joined = usp_is_joined,
	.get_extension = rzi_lorawan_usp_get_extension,
};
