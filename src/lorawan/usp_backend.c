/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/lorawan_lbm/lorawan_hal_init.h>
#include <zephyr/usp/smtc_sw_platform_helper.h>
#include <smtc_modem_api.h>
#include <smtc_modem_utilities.h>
#include <smtc_modem_hal.h>
#include <rzi/lorawan.h>

#define STACK_ID 0
/* usp_zephyr currently leaves registration to its integrating application. */
LOG_MODULE_REGISTER(usp, CONFIG_USP_LOG_LEVEL);
BUILD_ASSERT(CONFIG_USP_THREADS_MUTEXES, "RZI needs USP serialization");
BUILD_ASSERT(RZI_LORAWAN_MAX_PAYLOAD == SMTC_MODEM_MAX_LORAWAN_PAYLOAD_LENGTH);

K_MSGQ_DEFINE(events, sizeof(struct rzi_lorawan_event),
	      CONFIG_RZI_LORAWAN_EVENT_QUEUE_SIZE, 4);
static atomic_t started;
static atomic_t overflow;
/* Protected by rac_api_mutex, also held by the USP engine callback. */
static bool ready;
static bool tx_pending;
static struct rzi_lorawan_config settings;
static smtc_modem_region_t region;

static int result(smtc_modem_return_code_t rc)
{
	switch (rc) {
	case SMTC_MODEM_RC_OK: return 0;
	case SMTC_MODEM_RC_NOT_INIT: return -EAGAIN;
	case SMTC_MODEM_RC_INVALID:
	case SMTC_MODEM_RC_INVALID_STACK_ID: return -EINVAL;
	case SMTC_MODEM_RC_BUSY: return -EBUSY;
	case SMTC_MODEM_RC_NO_TIME: return -EAGAIN;
	case SMTC_MODEM_RC_NO_EVENT: return -EAGAIN;
	default: return -EIO;
	}
}

static int map_region(enum rzi_lorawan_region value, smtc_modem_region_t *out)
{
	switch (value) {
#define REGION(name) case RZI_LORAWAN_REGION_##name: *out = SMTC_MODEM_REGION_##name; return 0
	REGION(EU_868);
	REGION(US_915);
	REGION(AU_915);
	REGION(CN_470);
	REGION(AS_923_GRP1);
	REGION(AS_923_GRP2);
	REGION(AS_923_GRP3);
	REGION(AS_923_GRP4);
	REGION(IN_865);
	REGION(KR_920);
	REGION(RU_864);
#undef REGION
	default: return -EINVAL;
	}
}

static void publish(const struct rzi_lorawan_event *event)
{
	if (k_msgq_put(&events, event, K_NO_WAIT) != 0) {
		atomic_set(&overflow, 1);
	}
}

static void publish_error(int error)
{
	struct rzi_lorawan_event event = {.type = RZI_LORAWAN_ERROR, .error = error};
	publish(&event);
}

static int configure(void)
{
	int rc;
#define CONFIGURE(call) do { rc = result(call); if (rc) { return rc; } } while (0)
	CONFIGURE(smtc_modem_set_deveui(STACK_ID, settings.dev_eui));
	CONFIGURE(smtc_modem_set_joineui(STACK_ID, settings.join_eui));
	CONFIGURE(smtc_modem_set_appkey(STACK_ID, settings.application_key));
	CONFIGURE(smtc_modem_set_nwkkey(STACK_ID, settings.network_key));
	CONFIGURE(smtc_modem_set_region(STACK_ID, region));
	CONFIGURE(smtc_modem_set_join_duty_cycle_backoff_bypass(
		STACK_ID, settings.join_backoff_bypass));
#undef CONFIGURE
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
			ready = false;
			tx_pending = false;
			int error = configure();
			if (error) {
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
				event.tx_result = RZI_LORAWAN_TX_ACKED; break;
			case SMTC_MODEM_EVENT_TXDONE_SENT:
				event.tx_result = RZI_LORAWAN_TX_SENT; break;
			default: event.tx_result = RZI_LORAWAN_TX_NOT_SENT; break;
			}
			publish(&event);
			break;
		case SMTC_MODEM_EVENT_DOWNDATA: {
			uint8_t remaining = 0;
			do {
				smtc_modem_dl_metadata_t meta = {0};
				event.type = RZI_LORAWAN_DOWNLINK;
				rc = smtc_modem_get_downlink_data(event.downlink.data,
					&event.downlink.size, &meta, &remaining);
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
		default: break;
		}
	} while (pending > 0);
}

int rzi_lorawan_init(const struct rzi_lorawan_config *config)
{
	smtc_modem_region_t selected;
	if (k_is_in_isr()) { return -EWOULDBLOCK; }
	if (!config || map_region(config->region, &selected)) { return -EINVAL; }
	if (!atomic_cas(&started, 0, 1)) { return -EALREADY; }
	settings = *config;
	region = selected;
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
	if (k_is_in_isr()) { return -EWOULDBLOCK; }
	if (!atomic_get(&started)) { return -EAGAIN; }
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

int rzi_lorawan_join(void)
{
	int rc = enter();
	if (rc) { return rc; }
	return finish(result(smtc_modem_join_network(STACK_ID)));
}

int rzi_lorawan_leave(void)
{
	int rc = enter();
	if (rc) { return rc; }
	/* Preserve completion ownership: do not cancel an outstanding TX. */
	if (tx_pending) { return finish(-EBUSY); }
	return finish(result(smtc_modem_leave_network(STACK_ID)));
}

int rzi_lorawan_send(uint8_t port, const uint8_t *data, size_t size, bool confirmed)
{
	if (!data || size > RZI_LORAWAN_MAX_PAYLOAD || port == 0 || port > 223) {
		return -EINVAL;
	}
	int rc = enter();
	if (rc) { return rc; }
	if (tx_pending) { return finish(-EBUSY); }
	rc = result(smtc_modem_request_uplink(STACK_ID, port, confirmed, data, size));
	if (!rc) { tx_pending = true; }
	return finish(rc);
}

int rzi_lorawan_is_joined(bool *joined)
{
	if (!joined) { return -EINVAL; }
	int rc = enter();
	if (rc) { return rc; }
	smtc_modem_status_mask_t status;
	rc = result(smtc_modem_get_status(STACK_ID, &status));
	if (!rc) { *joined = (status & SMTC_MODEM_STATUS_JOINED) != 0; }
	return finish(rc);
}

int rzi_lorawan_get_event(struct rzi_lorawan_event *event, int32_t timeout_ms)
{
	if (k_is_in_isr()) { return -EWOULDBLOCK; }
	if (!event || timeout_ms < -1) { return -EINVAL; }
	if (!atomic_get(&started)) { return -EAGAIN; }
	if (atomic_cas(&overflow, 1, 0)) { return -EOVERFLOW; }
	int rc = k_msgq_get(&events, event,
		timeout_ms == -1 ? K_FOREVER : K_MSEC(timeout_ms));
	return rc ? -EAGAIN : 0;
}
