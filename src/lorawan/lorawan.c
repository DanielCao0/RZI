// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <rzi/lorawan.h>

#include "backend.h"

K_MSGQ_DEFINE(events, sizeof(struct rzi_lorawan_event),
	      CONFIG_RZI_LORAWAN_EVENT_QUEUE_SIZE, 4);

static atomic_t initialized;
static atomic_t overflow;

static void publish(const struct rzi_lorawan_event *event)
{
	if (k_msgq_put(&events, event, K_NO_WAIT) != 0) {
		atomic_set(&overflow, 1);
	}
}

static int check_context(void)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if (!atomic_get(&initialized)) {
		return -EAGAIN;
	}
	return 0;
}

int rzi_lorawan_init(const struct rzi_lorawan_config *config)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if (config == NULL || config->region < RZI_LORAWAN_REGION_EU_868 ||
	    config->region > RZI_LORAWAN_REGION_RU_864) {
		return -EINVAL;
	}
	if (!atomic_cas(&initialized, 0, 1)) {
		return -EALREADY;
	}

	rc = rzi_lorawan_backend.init(config, publish);
	if (rc != 0) {
		atomic_clear(&initialized);
	}
	return rc;
}

int rzi_lorawan_join(void)
{
	int rc = check_context();

	return rc != 0 ? rc : rzi_lorawan_backend.join();
}

int rzi_lorawan_leave(void)
{
	int rc = check_context();

	return rc != 0 ? rc : rzi_lorawan_backend.leave();
}

int rzi_lorawan_send(uint8_t port, const uint8_t *data, size_t size, bool confirmed)
{
	int rc;

	if (data == NULL || size > RZI_LORAWAN_MAX_PAYLOAD || port == 0 || port > 223) {
		return -EINVAL;
	}
	rc = check_context();
	return rc != 0 ? rc : rzi_lorawan_backend.send(port, data, size, confirmed);
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

int rzi_lorawan_get_event(struct rzi_lorawan_event *event, int32_t timeout_ms)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if (event == NULL || timeout_ms < -1) {
		return -EINVAL;
	}
	if (!atomic_get(&initialized)) {
		return -EAGAIN;
	}
	if (atomic_cas(&overflow, 1, 0)) {
		return -EOVERFLOW;
	}

	rc = k_msgq_get(&events, event,
			timeout_ms == -1 ? K_FOREVER : K_MSEC(timeout_ms));
	return rc != 0 ? -EAGAIN : 0;
}
