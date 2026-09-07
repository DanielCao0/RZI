/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <rzi/at/uart.h>

#define CONSOLE_NODE DT_CHOSEN(zephyr_console)

static const struct device *const at_uart = DEVICE_DT_GET(CONSOLE_NODE);

int main(void)
{
	if (!device_is_ready(at_uart)) {
		return -ENODEV;
	}

	/* Give a USB CDC host time to open the port before the service
	 * starts; on a plain UART this returns immediately.
	 */
	if (DT_NODE_HAS_COMPAT(CONSOLE_NODE, zephyr_cdc_acm_uart)) {
		uint32_t dtr = 0;

		for (int i = 0; i < 100 && dtr == 0; i++) {
			(void)uart_line_ctrl_get(at_uart, UART_LINE_CTRL_DTR, &dtr);
			k_msleep(100);
		}
	}

	/* The AT service owns the UART from here on. */
	return rzi_at_uart_start(at_uart);
}
