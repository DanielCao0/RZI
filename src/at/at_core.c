/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdarg.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/ring_buffer.h>

#include <rzi/at.h>

#include "at_priv.h"

RING_BUF_DECLARE(rx_ring, CONFIG_RZI_AT_RX_BUFFER_SIZE);
K_SEM_DEFINE(rx_ready, 0, 1);
K_MUTEX_DEFINE(tx_lock);
K_THREAD_STACK_DEFINE(at_stack, CONFIG_RZI_AT_THREAD_STACK_SIZE);

static struct k_thread at_thread;
static struct rzi_at_transport active_transport;
static atomic_t started;
static atomic_t rx_overflow;

static int write_all(const char *text)
{
	size_t size = strlen(text);
	int rc;

	k_mutex_lock(&tx_lock, K_FOREVER);
	rc = active_transport.write((const uint8_t *)text, size, active_transport.user_data);
	k_mutex_unlock(&tx_lock);
	return rc;
}

int rzi_at_write_raw(const char *text)
{
	if (!atomic_get(&started) || text == NULL) {
		return -EINVAL;
	}
	return write_all(text);
}

int rzi_at_respond_status(enum rzi_at_status status)
{
	static const char *const names[] = {
		[RZI_AT_STATUS_OK] = "OK",
		[RZI_AT_STATUS_ERROR] = "AT_ERROR",
		[RZI_AT_STATUS_PARAM_ERROR] = "AT_PARAM_ERROR",
		[RZI_AT_STATUS_BUSY_ERROR] = "AT_BUSY_ERROR",
		[RZI_AT_STATUS_NO_NETWORK_JOINED] = "AT_NO_NETWORK_JOINED",
		[RZI_AT_STATUS_TEST_PARAM_OVERFLOW] = "AT_TEST_PARAM_OVERFLOW",
	};
	char buffer[48];

	if (!atomic_get(&started) || status < 0 || status >= ARRAY_SIZE(names)) {
		return -EINVAL;
	}
	(void)snprintk(buffer, sizeof(buffer), "\r\n%s\r\n", names[status]);
	return write_all(buffer);
}

static int write_formatted(const char *prefix, const char *suffix, const char *format, va_list args)
{
	char buffer[CONFIG_RZI_AT_TX_BUFFER_SIZE];
	int head;
	int body;

	head = snprintk(buffer, sizeof(buffer), "%s", prefix);
	if (head < 0 || head >= sizeof(buffer)) {
		return -ENOSPC;
	}
	body = vsnprintk(buffer + head, sizeof(buffer) - head, format, args);
	if (body < 0 || body >= sizeof(buffer) - head) {
		return -ENOSPC;
	}
	size_t used = (size_t)head + (size_t)body;
	size_t suffix_size = strlen(suffix);

	if (used + suffix_size >= sizeof(buffer)) {
		return -ENOSPC;
	}
	memcpy(buffer + used, suffix, suffix_size + 1U);
	return write_all(buffer);
}

int rzi_at_respond_value(const char *format, ...)
{
	va_list args;
	int rc;

	if (!atomic_get(&started) || format == NULL) {
		return -EINVAL;
	}
	va_start(args, format);
	rc = write_formatted("\r\n", "\r\nOK\r\n", format, args);
	va_end(args);
	return rc;
}

int rzi_at_publish_event(const char *format, ...)
{
	va_list args;
	int rc;

	if (!atomic_get(&started) || format == NULL) {
		return -EINVAL;
	}
	va_start(args, format);
	rc = write_formatted("\r\n+EVT:", "\r\n", format, args);
	va_end(args);
	return rc;
}

int rzi_at_receive(const uint8_t *data, size_t size)
{
	uint32_t written;

	if (!atomic_get(&started)) {
		return -EAGAIN;
	}
	if (data == NULL && size != 0U) {
		return -EINVAL;
	}
	written = ring_buf_put(&rx_ring, data, size);
	if (written != size) {
		atomic_set(&rx_overflow, 1);
	}
	k_sem_give(&rx_ready);
	return written == size ? 0 : -ENOSPC;
}

static void process_input(void)
{
	uint8_t buffer[64];
	uint32_t count;

	if (atomic_cas(&rx_overflow, 1, 0)) {
		rzi_at_parser_reset();
		(void)rzi_at_respond_status(RZI_AT_STATUS_TEST_PARAM_OVERFLOW);
	}
	while ((count = ring_buf_get(&rx_ring, buffer, sizeof(buffer))) != 0U) {
		for (uint32_t i = 0; i < count; ++i) {
			(void)rzi_at_parser_feed(buffer[i]);
		}
	}
}

static void thread_entry(void *unused1, void *unused2, void *unused3)
{
	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);
	ARG_UNUSED(unused3);

	for (;;) {
		(void)k_sem_take(&rx_ready, K_MSEC(CONFIG_RZI_AT_PROCESS_INTERVAL_MS));
		process_input();
		rzi_at_registry_process_extensions();
	}
}

int rzi_at_start(const struct rzi_at_transport *transport)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if (transport == NULL || transport->write == NULL) {
		return -EINVAL;
	}
	if (!atomic_cas(&started, 0, 1)) {
		return -EALREADY;
	}
	active_transport = *transport;
	rc = rzi_at_builtin_register();
	if (rc == 0) {
		rzi_at_registry_seal();
		rc = rzi_at_registry_start_extensions();
	}
	if (rc != 0) {
		atomic_clear(&started);
		return rc;
	}
	k_thread_create(&at_thread, at_stack, K_THREAD_STACK_SIZEOF(at_stack), thread_entry, NULL,
			NULL, NULL, K_PRIO_PREEMPT(CONFIG_RZI_AT_THREAD_PRIORITY), 0, K_NO_WAIT);
	k_thread_name_set(&at_thread, "rzi_at");
	return 0;
}
