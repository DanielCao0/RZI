// SPDX-License-Identifier: Apache-2.0
/**
 * @file
 * @brief Backend-independent RZI LoRaWAN service implementation.
 */

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <rzi/lorawan.h>

#include "lorawan_backend.h"

#ifdef CONFIG_RZI_LORAWAN_FUOTA
#include <rzi/fuota.h>
#include "services/fuota/lorawan_fuota.h"

static void ignore_result(int result)
{
	ARG_UNUSED(result);
}
#endif

K_MSGQ_DEFINE(events, sizeof(struct rzi_lorawan_backend_event), CONFIG_RZI_LORAWAN_EVENT_QUEUE_SIZE,
	      4);
K_MUTEX_DEFINE(callbacks_lock);
K_MUTEX_DEFINE(config_lock);

struct callback_slot {
	bool active;
	rzi_lorawan_callback_handle_t handle;
	struct rzi_lorawan_callbacks callbacks;
};

static struct callback_slot callback_slots[CONFIG_RZI_LORAWAN_MAX_CALLBACKS];
static rzi_lorawan_callback_handle_t next_callback_handle = 1U;
static enum rzi_lorawan_region configured_region = RZI_LORAWAN_REGION_EU_868;
static bool configured_join_backoff_bypass;
static atomic_t started;
static atomic_t overflow;

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

static void dispatch(const struct rzi_lorawan_backend_event *event)
{
	struct callback_slot subscribers[CONFIG_RZI_LORAWAN_MAX_CALLBACKS] = {0};
	size_t count = 0;

	k_mutex_lock(&callbacks_lock, K_FOREVER);
	for (size_t i = 0; i < ARRAY_SIZE(callback_slots); ++i) {
		if (callback_slots[i].active) {
			subscribers[count++] = callback_slots[i];
		}
	}
	k_mutex_unlock(&callbacks_lock);

	for (size_t i = 0; i < count; ++i) {
		const struct rzi_lorawan_callbacks *callbacks = &subscribers[i].callbacks;
		void *user_data = callbacks->user_data;

		switch (event->type) {
		case RZI_LORAWAN_BACKEND_READY:
			if (callbacks->state_changed != NULL) {
				callbacks->state_changed(RZI_LORAWAN_STATE_READY, user_data);
			}
			break;
		case RZI_LORAWAN_BACKEND_JOINED:
		case RZI_LORAWAN_BACKEND_JOIN_FAILED:
			if (callbacks->state_changed != NULL) {
				callbacks->state_changed(event->type == RZI_LORAWAN_BACKEND_JOINED
								 ? RZI_LORAWAN_STATE_JOINED
								 : RZI_LORAWAN_STATE_READY,
							 user_data);
			}
			if (callbacks->join_done != NULL) {
				int status = 0;

				if (event->type == RZI_LORAWAN_BACKEND_JOIN_FAILED) {
					status = event->error != 0 ? event->error : -ETIMEDOUT;
				}
				callbacks->join_done(status, user_data);
			}
			break;
		case RZI_LORAWAN_BACKEND_TX_DONE:
			if (callbacks->send_done != NULL) {
				const struct rzi_lorawan_tx_result result = {
					.status = event->tx_status,
					.error = 0,
				};

				callbacks->send_done(&result, user_data);
			}
			break;
		case RZI_LORAWAN_BACKEND_DOWNLINK:
			if (callbacks->downlink != NULL) {
				const struct rzi_lorawan_downlink downlink = {
					.port = event->downlink.port,
					.size = event->downlink.size,
					.rssi_dbm = event->downlink.rssi_dbm,
					.snr_quarter_db = event->downlink.snr_quarter_db,
					.flags = event->downlink.flags,
					.data = event->downlink.data,
				};

				callbacks->downlink(&downlink, user_data);
			}
			break;
		case RZI_LORAWAN_BACKEND_ERROR:
			if (callbacks->error != NULL) {
				callbacks->error(event->error, user_data);
			}
			break;
		case RZI_LORAWAN_BACKEND_STATE_CHANGED:
			if (callbacks->state_changed != NULL) {
				callbacks->state_changed(event->state, user_data);
			}
			break;
		case RZI_LORAWAN_BACKEND_FUOTA:
			break;
		}
	}
#ifdef CONFIG_RZI_LORAWAN_FUOTA
	rzi_lorawan_fuota_on_backend_event(event);
#endif
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
				.error = -EOVERFLOW,
			};

			dispatch(&error);
		}
	}
}

K_THREAD_DEFINE(rzi_lorawan_dispatcher, CONFIG_RZI_LORAWAN_DISPATCHER_STACK_SIZE, dispatcher, NULL,
		NULL, NULL, CONFIG_RZI_LORAWAN_DISPATCHER_PRIORITY, 0, 0);

static int check_context(void)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if (!atomic_get(&started)) {
		return -EAGAIN;
	}
	return 0;
}

static bool callbacks_empty(const struct rzi_lorawan_callbacks *callbacks)
{
	return callbacks->join_done == NULL && callbacks->send_done == NULL &&
	       callbacks->downlink == NULL && callbacks->state_changed == NULL &&
	       callbacks->error == NULL;
}

int rzi_lorawan_register_callbacks(const struct rzi_lorawan_callbacks *callbacks,
				   rzi_lorawan_callback_handle_t *handle)
{
	struct callback_slot *available = NULL;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if (callbacks == NULL || handle == NULL || callbacks_empty(callbacks)) {
		return -EINVAL;
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
		return -ENOMEM;
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
	int rc = -ENOENT;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if (handle == RZI_LORAWAN_CALLBACK_HANDLE_INVALID) {
		return -EINVAL;
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
		return -EWOULDBLOCK;
	}
	if ((unsigned int)region > RZI_LORAWAN_REGION_RU_864) {
		return -EINVAL;
	}

	k_mutex_lock(&config_lock, K_FOREVER);
	if (atomic_get(&started)) {
		rc = -EALREADY;
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
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&config_lock, K_FOREVER);
	if (atomic_get(&started)) {
		rc = -EALREADY;
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
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&config_lock, K_FOREVER);
	if (!atomic_cas(&started, 0, 1)) {
		k_mutex_unlock(&config_lock);
		return -EALREADY;
	}
	region = configured_region;
	bypass = configured_join_backoff_bypass;
	k_mutex_unlock(&config_lock);

	publish_state(RZI_LORAWAN_STATE_STARTING);
	rc = rzi_lorawan_backend.start(region, bypass, publish);
	if (rc != 0) {
		atomic_clear(&started);
		publish_state(RZI_LORAWAN_STATE_STOPPED);
#ifdef CONFIG_RZI_LORAWAN_FUOTA
	} else {
		ignore_result(rzi_fuota_start());
#endif
	}
	return rc;
}

int rzi_lorawan_join(const struct rzi_lorawan_join_config *config)
{
	int rc;

	if (config == NULL || (config->activation != RZI_LORAWAN_ACTIVATION_OTAA &&
			       config->activation != RZI_LORAWAN_ACTIVATION_ABP)) {
		return -EINVAL;
	}
	rc = check_context();
	if (rc != 0) {
		return rc;
	}
	if (config->activation == RZI_LORAWAN_ACTIVATION_OTAA &&
	    !(rzi_lorawan_backend.capabilities & RZI_LORAWAN_CAP_OTAA)) {
		return -ENOTSUP;
	}
	if (config->activation == RZI_LORAWAN_ACTIVATION_ABP &&
	    !(rzi_lorawan_backend.capabilities & RZI_LORAWAN_CAP_ABP)) {
		return -ENOTSUP;
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
			publish_state(RZI_LORAWAN_STATE_READY);
		}
	}
	return rc;
}

int rzi_lorawan_send(uint8_t port, const uint8_t *data, size_t size,
		     enum rzi_lorawan_message_type type)
{
	int rc;

	if ((data == NULL && size != 0U) || size > RZI_LORAWAN_MAX_PAYLOAD || port == 0 ||
	    port > 223 ||
	    (type != RZI_LORAWAN_MSG_UNCONFIRMED && type != RZI_LORAWAN_MSG_CONFIRMED)) {
		return -EINVAL;
	}
	rc = check_context();
	return rc != 0 ? rc : rzi_lorawan_backend.send(port, data, size, type);
}

int rzi_lorawan_set_class(enum rzi_lorawan_class device_class)
{
	int rc;

	if ((unsigned int)device_class > RZI_LORAWAN_CLASS_C) {
		return -EINVAL;
	}
	rc = check_context();
	return rc != 0 ? rc : rzi_lorawan_backend.set_class(device_class);
}

int rzi_lorawan_is_joined(bool *joined)
{
	int rc;

	if (joined == NULL) {
		return -EINVAL;
	}
	rc = check_context();
	return rc != 0 ? rc : rzi_lorawan_backend.is_joined(joined);
}

uint32_t rzi_lorawan_get_capabilities(void)
{
	return rzi_lorawan_backend.capabilities;
}
