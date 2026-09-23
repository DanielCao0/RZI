/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Backend-independent RZI LoRaWAN service implementation.
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <rzi/lorawan/lorawan.h>

#if defined(CONFIG_RZI_POWER) && defined(CONFIG_RZI_POWER_AUTO_SERVICE_BLOCK)
#include <rzi/power/power.h>
#endif

#include "backend/lorawan_backend.h"
#include "lorawan_priv.h"
#include "lorawan_service.h"

#define SERVICE_MAX 2U

K_MSGQ_DEFINE(events, sizeof(struct rzi_lorawan_backend_event), CONFIG_RZI_LORAWAN_EVENT_QUEUE_SIZE,
	      4);
K_MUTEX_DEFINE(callbacks_lock);
K_MUTEX_DEFINE(config_lock);
K_MUTEX_DEFINE(services_lock);

static const struct rzi_lorawan_service *services[SERVICE_MAX];
static size_t service_count;

struct callback_slot {
	bool active;
	rzi_lorawan_callback_handle_t handle;
	struct rzi_lorawan_callbacks callbacks;
};

static struct callback_slot callback_slots[CONFIG_RZI_LORAWAN_MAX_CALLBACKS];
static rzi_lorawan_callback_handle_t next_callback_handle = 1U;
static enum rzi_lorawan_region configured_region = RZI_LORAWAN_REGION_EU_868;
static enum rzi_lorawan_class configured_class = RZI_LORAWAN_CLASS_A;
static bool configured_join_backoff_bypass;
static atomic_t started;
static atomic_t overflow;
static atomic_t tx_outstanding;
static bool last_downlink_valid;
static int16_t last_rssi_dbm;
static int8_t last_snr_quarter_db;
static const char protocol_version[] = "LoRaWAN 1.0.4";
#if defined(CONFIG_RZI_POWER) && defined(CONFIG_RZI_POWER_AUTO_SERVICE_BLOCK)
static bool power_class_c_held;
#endif

#if defined(CONFIG_RZI_POWER) && defined(CONFIG_RZI_POWER_AUTO_SERVICE_BLOCK)
static void power_sync_class_c(bool class_c)
{
	if (class_c && !power_class_c_held && rzi_power_block(RZI_POWER_BLOCK_LORAWAN) == 0) {
		power_class_c_held = true;
	} else if (!class_c && power_class_c_held) {
		int ignored = rzi_power_unblock(RZI_POWER_BLOCK_LORAWAN);

		ARG_UNUSED(ignored);
		power_class_c_held = false;
	}
}
#else
static void power_sync_class_c(bool class_c)
{
	ARG_UNUSED(class_c);
}
#endif

static void publish(const struct rzi_lorawan_backend_event *event)
{
	if (k_msgq_put(&events, event, K_NO_WAIT) != 0) {
		atomic_set(&overflow, 1);
	}
}

static void publish_state(enum rzi_lorawan_state state)
{
	const struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_STATE_CHANGED,
		.state = state,
	};

	publish(&event);
}

static bool event_from_backend(const struct rzi_lorawan_backend_event *event,
			       struct rzi_lorawan_event *out)
{
	memset(out, 0, sizeof(*out));
	switch (event->type) {
	case RZI_LORAWAN_BACKEND_READY:
		out->type = RZI_LORAWAN_EVENT_READY;
		return true;
	case RZI_LORAWAN_BACKEND_JOINED:
		out->type = RZI_LORAWAN_EVENT_JOINED;
		return true;
	case RZI_LORAWAN_BACKEND_JOIN_FAILED:
		out->type = RZI_LORAWAN_EVENT_JOIN_FAILED;
		out->error = event->error != 0 ? event->error : -RZI_ERR_TIMEOUT;
		return true;
	case RZI_LORAWAN_BACKEND_TX_DONE:
		atomic_clear(&tx_outstanding);
		out->type = RZI_LORAWAN_EVENT_TX_DONE;
		out->tx.status = event->tx_status;
		out->tx.error = event->error;
		return true;
	case RZI_LORAWAN_BACKEND_DOWNLINK:
		last_rssi_dbm = event->downlink.rssi_dbm;
		last_snr_quarter_db = event->downlink.snr_quarter_db;
		last_downlink_valid = true;
		out->type = RZI_LORAWAN_EVENT_DOWNLINK;
		out->downlink.port = event->downlink.port;
		out->downlink.size = event->downlink.size;
		out->downlink.rssi_dbm = event->downlink.rssi_dbm;
		out->downlink.snr_quarter_db = event->downlink.snr_quarter_db;
		out->downlink.flags = event->downlink.flags;
		out->downlink.data = event->downlink.data;
		return true;
	case RZI_LORAWAN_BACKEND_ERROR:
		out->type = RZI_LORAWAN_EVENT_ERROR;
		out->error = event->error;
		return true;
	case RZI_LORAWAN_BACKEND_STATE_CHANGED:
		out->type = RZI_LORAWAN_EVENT_STATE_CHANGED;
		out->state = event->state;
		return true;
	case RZI_LORAWAN_BACKEND_LINK_CHECK:
		out->type = RZI_LORAWAN_EVENT_LINK_CHECK;
		out->link_check = event->link_check;
		return true;
	case RZI_LORAWAN_BACKEND_DEVICE_TIME:
		out->type = RZI_LORAWAN_EVENT_DEVICE_TIME;
		out->error = event->error;
		return true;
	case RZI_LORAWAN_BACKEND_CLASS_B:
		out->type = RZI_LORAWAN_EVENT_CLASS_B;
		out->class_b = event->class_b;
		return true;
	case RZI_LORAWAN_BACKEND_FUOTA:
	default:
		return false;
	}
}

static void dispatch(const struct rzi_lorawan_backend_event *event)
{
	struct callback_slot subscribers[CONFIG_RZI_LORAWAN_MAX_CALLBACKS] = {0};
	const struct rzi_lorawan_service *attached[SERVICE_MAX];
	struct rzi_lorawan_event public_event;
	size_t count = 0;
	size_t attached_count;
	bool deliver;

	k_mutex_lock(&callbacks_lock, K_FOREVER);
	for (size_t i = 0; i < ARRAY_SIZE(callback_slots); ++i) {
		if (callback_slots[i].active) {
			subscribers[count++] = callback_slots[i];
		}
	}
	k_mutex_unlock(&callbacks_lock);

	deliver = event_from_backend(event, &public_event);
	for (size_t i = 0; i < count; ++i) {
		const struct rzi_lorawan_callbacks *callbacks = &subscribers[i].callbacks;

		if (deliver && callbacks->on_event != NULL) {
			callbacks->on_event(&public_event, callbacks->user_data);
		}
	}

	k_mutex_lock(&services_lock, K_FOREVER);
	attached_count = service_count;
	memcpy(attached, services, attached_count * sizeof(attached[0]));
	k_mutex_unlock(&services_lock);
	for (size_t i = 0; i < attached_count; ++i) {
		if (attached[i]->on_event != NULL) {
			attached[i]->on_event(event);
		}
	}
}

static void dispatcher(void *unused1, void *unused2, void *unused3)
{
	struct rzi_lorawan_backend_event event;

	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);
	ARG_UNUSED(unused3);

	for (;;) {
		k_msgq_get(&events, &event, K_FOREVER);
		dispatch(&event);
		if (atomic_cas(&overflow, 1, 0)) {
			const struct rzi_lorawan_backend_event error = {
				.type = RZI_LORAWAN_BACKEND_ERROR,
				.error = -RZI_ERR_OVERFLOW,
			};

			dispatch(&error);
		}
	}
}

K_THREAD_DEFINE(rzi_lorawan_dispatcher, CONFIG_RZI_LORAWAN_DISPATCHER_STACK_SIZE, dispatcher, NULL,
		NULL, NULL, CONFIG_RZI_LORAWAN_DISPATCHER_PRIORITY, 0, 0);

int rzi_lorawan_check_thread(void)
{
	return k_is_in_isr() ? -RZI_ERR_WOULDBLOCK : 0;
}

int rzi_lorawan_check_started(void)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (!atomic_get(&started)) {
		return -RZI_ERR_NOT_READY;
	}
	return 0;
}

static int check_context(void)
{
	return rzi_lorawan_check_started();
}

void rzi_lorawan_note_class(enum rzi_lorawan_class device_class)
{
	k_mutex_lock(&config_lock, K_FOREVER);
	configured_class = device_class;
	k_mutex_unlock(&config_lock);
	power_sync_class_c(device_class == RZI_LORAWAN_CLASS_C);
}

static bool callbacks_empty(const struct rzi_lorawan_callbacks *callbacks)
{
	return callbacks->on_event == NULL;
}

int rzi_lorawan_register_callbacks(const struct rzi_lorawan_callbacks *callbacks,
				   rzi_lorawan_callback_handle_t *handle)
{
	struct callback_slot *available = NULL;

	if (k_is_in_isr()) {
		return -RZI_ERR_WOULDBLOCK;
	}
	if (callbacks == NULL || handle == NULL || callbacks_empty(callbacks)) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&callbacks_lock, K_FOREVER);
	for (size_t i = 0; i < ARRAY_SIZE(callback_slots); ++i) {
		if (!callback_slots[i].active) {
			available = &callback_slots[i];
			break;
		}
	}
	if (available == NULL) {
		k_mutex_unlock(&callbacks_lock);
		return -RZI_ERR_NO_RESOURCE;
	}

	available->callbacks = *callbacks;
	available->handle = next_callback_handle++;
	if (next_callback_handle == RZI_LORAWAN_CALLBACK_HANDLE_INVALID) {
		next_callback_handle++;
	}
	available->active = true;
	*handle = available->handle;
	k_mutex_unlock(&callbacks_lock);
	return 0;
}

int rzi_lorawan_unregister_callbacks(rzi_lorawan_callback_handle_t handle)
{
	int rc = -RZI_ERR_NOT_FOUND;

	if (k_is_in_isr()) {
		return -RZI_ERR_WOULDBLOCK;
	}
	if (handle == RZI_LORAWAN_CALLBACK_HANDLE_INVALID) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&callbacks_lock, K_FOREVER);
	for (size_t i = 0; i < ARRAY_SIZE(callback_slots); ++i) {
		if (callback_slots[i].active && callback_slots[i].handle == handle) {
			memset(&callback_slots[i], 0, sizeof(callback_slots[i]));
			rc = 0;
			break;
		}
	}
	k_mutex_unlock(&callbacks_lock);
	return rc;
}

int rzi_lorawan_set_region(enum rzi_lorawan_region region)
{
	int rc = 0;

	if (k_is_in_isr()) {
		return -RZI_ERR_WOULDBLOCK;
	}
	if ((unsigned int)region > RZI_LORAWAN_REGION_RU_864) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&config_lock, K_FOREVER);
	if (atomic_get(&started)) {
		rc = -RZI_ERR_ALREADY;
	} else {
		configured_region = region;
	}
	k_mutex_unlock(&config_lock);
	return rc;
}

int rzi_lorawan_set_join_backoff_bypass(bool enabled)
{
	int rc = 0;

	if (k_is_in_isr()) {
		return -RZI_ERR_WOULDBLOCK;
	}

	k_mutex_lock(&config_lock, K_FOREVER);
	if (atomic_get(&started)) {
		rc = -RZI_ERR_ALREADY;
	} else {
		configured_join_backoff_bypass = enabled;
	}
	k_mutex_unlock(&config_lock);
	return rc;
}

int rzi_lorawan_start(void)
{
	enum rzi_lorawan_region region;
	bool bypass;
	int rc;

	if (k_is_in_isr()) {
		return -RZI_ERR_WOULDBLOCK;
	}

	k_mutex_lock(&config_lock, K_FOREVER);
	if (!atomic_cas(&started, 0, 1)) {
		k_mutex_unlock(&config_lock);
		return -RZI_ERR_ALREADY;
	}
	region = configured_region;
	bypass = configured_join_backoff_bypass;
	k_mutex_unlock(&config_lock);

	publish_state(RZI_LORAWAN_STATE_STARTING);
	rc = rzi_lorawan_backend.start(region, bypass, publish);
	if (rc != 0) {
		atomic_clear(&started);
		publish_state(RZI_LORAWAN_STATE_STOPPED);
	} else {
		const struct rzi_lorawan_service *attached[SERVICE_MAX];
		size_t attached_count;

		k_mutex_lock(&services_lock, K_FOREVER);
		attached_count = service_count;
		memcpy(attached, services, attached_count * sizeof(attached[0]));
		k_mutex_unlock(&services_lock);
		for (size_t i = 0; i < attached_count; ++i) {
			if (attached[i]->on_started != NULL) {
				attached[i]->on_started();
			}
		}
	}
	return rc;
}

int rzi_lorawan_register_service(const struct rzi_lorawan_service *service)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (service == NULL) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&services_lock, K_FOREVER);
	if (service_count >= ARRAY_SIZE(services)) {
		rc = -RZI_ERR_NO_RESOURCE;
	} else {
		services[service_count++] = service;
	}
	k_mutex_unlock(&services_lock);
	return rc;
}

int rzi_lorawan_join(const struct rzi_lorawan_join_config *config)
{
	int rc;

	if (config == NULL || (config->activation != RZI_LORAWAN_ACTIVATION_OTAA &&
			       config->activation != RZI_LORAWAN_ACTIVATION_ABP)) {
		return -RZI_ERR_INVALID;
	}
	rc = check_context();
	if (rc != 0) {
		return rc;
	}
	if (config->activation == RZI_LORAWAN_ACTIVATION_OTAA &&
	    !(rzi_lorawan_backend.capabilities & RZI_LORAWAN_CAP_OTAA)) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	if (config->activation == RZI_LORAWAN_ACTIVATION_ABP &&
	    !(rzi_lorawan_backend.capabilities & RZI_LORAWAN_CAP_ABP)) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	publish_state(RZI_LORAWAN_STATE_JOINING);
	rc = rzi_lorawan_backend.join(config);
	if (rc != 0) {
		publish_state(RZI_LORAWAN_STATE_READY);
	}
	return rc;
}

int rzi_lorawan_leave(void)
{
	int rc = check_context();

	if (rc == 0) {
		rc = rzi_lorawan_backend.leave();
		if (rc == 0) {
			power_sync_class_c(false);
			publish_state(RZI_LORAWAN_STATE_READY);
		}
	}
	return rc;
}

int rzi_lorawan_send(uint8_t port, const uint8_t *data, size_t size,
		     enum rzi_lorawan_message_type type)
{
	int rc;

	if ((data == NULL && size != 0U) || port == 0 || port > 223 ||
	    (type != RZI_LORAWAN_MSG_UNCONFIRMED && type != RZI_LORAWAN_MSG_CONFIRMED)) {
		return -RZI_ERR_INVALID;
	}
	if (size > RZI_LORAWAN_MAX_PAYLOAD) {
		return -RZI_ERR_TOO_LARGE;
	}
	rc = check_context();
	if (rc != 0) {
		return rc;
	}
	{
		bool joined = false;

		rc = rzi_lorawan_backend.is_joined(&joined);
		if (rc != 0) {
			return rc;
		}
		if (!joined) {
			return -RZI_ERR_NOT_JOINED;
		}
	}
	rzi_lorawan_mac_commands_on_uplink();
	rc = rzi_lorawan_backend.send(port, data, size, type);
	if (rc == 0) {
		atomic_set(&tx_outstanding, 1);
	}
	return rc;
}

int rzi_lorawan_set_class(enum rzi_lorawan_class device_class)
{
	int rc;

	if ((unsigned int)device_class > RZI_LORAWAN_CLASS_C) {
		return -RZI_ERR_INVALID;
	}
	rc = check_context();
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_backend.set_class(device_class);
	if (rc == 0) {
		rzi_lorawan_note_class(device_class);
	}
	return rc;
}

int rzi_lorawan_is_joined(bool *joined)
{
	int rc;

	if (joined == NULL) {
		return -RZI_ERR_INVALID;
	}
	rc = check_context();
	return rc != 0 ? rc : rzi_lorawan_backend.is_joined(joined);
}

uint32_t rzi_lorawan_get_capabilities(void)
{
	return rzi_lorawan_backend.capabilities;
}

int rzi_lorawan_get_region(enum rzi_lorawan_region *region)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (region == NULL) {
		return -RZI_ERR_INVALID;
	}
	k_mutex_lock(&config_lock, K_FOREVER);
	*region = configured_region;
	k_mutex_unlock(&config_lock);
	return 0;
}

int rzi_lorawan_get_class(enum rzi_lorawan_class *device_class)
{
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (device_class == NULL) {
		return -RZI_ERR_INVALID;
	}
	if (rzi_lorawan_backend.get_class != NULL) {
		rc = rzi_lorawan_backend.get_class(device_class);
		if (rc == 0) {
			rzi_lorawan_note_class(*device_class);
			return 0;
		}
		if (rc != -RZI_ERR_NOT_SUPPORTED) {
			return rc;
		}
	}
	k_mutex_lock(&config_lock, K_FOREVER);
	*device_class = configured_class;
	k_mutex_unlock(&config_lock);
	return 0;
}

int rzi_lorawan_get_last_rssi(int16_t *rssi_dbm)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (rssi_dbm == NULL) {
		return -RZI_ERR_INVALID;
	}
	if (!last_downlink_valid) {
		return -RZI_ERR_NO_DATA;
	}
	*rssi_dbm = last_rssi_dbm;
	return 0;
}

int rzi_lorawan_get_last_snr(int8_t *snr_quarter_db)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (snr_quarter_db == NULL) {
		return -RZI_ERR_INVALID;
	}
	if (!last_downlink_valid) {
		return -RZI_ERR_NO_DATA;
	}
	*snr_quarter_db = last_snr_quarter_db;
	return 0;
}

int rzi_lorawan_get_protocol_version(const char **version)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (version == NULL) {
		return -RZI_ERR_INVALID;
	}
	*version = protocol_version;
	return 0;
}

int rzi_lorawan_is_busy(bool *busy)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (busy == NULL) {
		return -RZI_ERR_INVALID;
	}
	if (rzi_lorawan_backend.is_busy != NULL) {
		rc = rzi_lorawan_backend.is_busy(busy);
		if (rc == 0) {
			return 0;
		}
		if (rc != -RZI_ERR_NOT_SUPPORTED) {
			return rc;
		}
	}
	*busy = atomic_get(&tx_outstanding) != 0;
	return 0;
}
