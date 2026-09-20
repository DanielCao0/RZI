/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Interrupt-driven UART I/O adapter for the RZI AT service.
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <rzi/at/at.h>
#include <rzi/at/uart.h>

static const struct device *active_uart;
static uint32_t current_baud = 115200U;

static void ignore_result(int result)
{
	ARG_UNUSED(result);
}

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
		ignore_result(rzi_at_receive(buffer, (size_t)count));
	}
}

int rzi_at_uart_start(const struct device *uart)
{
	struct rzi_at_io io = {
		.write = uart_write,
		.user_data = (void *)uart,
	};
	int rc;

	if (uart == NULL || !device_is_ready(uart)) {
		return -RZI_ERR_NO_DEVICE;
	}
	if (active_uart != NULL) {
		return -RZI_ERR_ALREADY;
	}
	rc = uart_irq_callback_user_data_set(uart, uart_isr, NULL);
	if (rc != 0) {
		return rc;
	}
	active_uart = uart;
	rc = rzi_at_start(&io);
	if (rc != 0) {
		active_uart = NULL;
		(void)uart_irq_callback_user_data_set(uart, NULL, NULL);
		return rc;
	}
	uart_irq_rx_enable(uart);
	return 0;
}

int rzi_at_uart_get_baud(uint32_t *baud_rate)
{
	if (baud_rate == NULL) {
		return -RZI_ERR_INVALID;
	}
	if (active_uart == NULL) {
		return -RZI_ERR_NO_DEVICE;
	}
	*baud_rate = current_baud;
	return 0;
}

int rzi_at_uart_set_baud(uint32_t baud_rate)
{
	struct uart_config config;
	int rc;

	if (baud_rate == 0U) {
		return -RZI_ERR_INVALID;
	}
	if (active_uart == NULL) {
		return -RZI_ERR_NO_DEVICE;
	}
	rc = uart_config_get(active_uart, &config);
	if (rc != 0) {
		return rc;
	}
	config.baudrate = baud_rate;
	rc = uart_configure(active_uart, &config);
	if (rc != 0) {
		return rc;
	}
	current_baud = baud_rate;
	return 0;
}
