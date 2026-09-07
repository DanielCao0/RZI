/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <rzi/at.h>
#include <rzi/at/uart.h>

static const struct device *active_uart;

static int uart_write(const uint8_t *data, size_t size, void *user_data)
{
	const struct device *uart = user_data;

	for (size_t i = 0; i < size; ++i) {
		uart_poll_out(uart, data[i]);
	}
	return 0;
}

static void uart_isr(const struct device *uart, void *user_data)
{
	uint8_t buffer[32];

	ARG_UNUSED(user_data);
	uart_irq_update(uart);
	while (uart_irq_rx_ready(uart)) {
		int count = uart_fifo_read(uart, buffer, sizeof(buffer));

		if (count <= 0) {
			break;
		}
		(void)rzi_at_receive(buffer, (size_t)count);
	}
}

int rzi_at_uart_start(const struct device *uart)
{
	struct rzi_at_transport transport = {
		.write = uart_write,
		.user_data = (void *)uart,
	};
	int rc;

	if (uart == NULL || !device_is_ready(uart)) {
		return -ENODEV;
	}
	if (active_uart != NULL) {
		return -EALREADY;
	}
	rc = uart_irq_callback_user_data_set(uart, uart_isr, NULL);
	if (rc != 0) {
		return rc;
	}
	active_uart = uart;
	rc = rzi_at_start(&transport);
	if (rc != 0) {
		active_uart = NULL;
		(void)uart_irq_callback_user_data_set(uart, NULL, NULL);
		return rc;
	}
	uart_irq_rx_enable(uart);
	return 0;
}
