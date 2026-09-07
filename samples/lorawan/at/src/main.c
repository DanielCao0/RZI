/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <rzi/at/uart.h>

#define AT_UART_NODE DT_ALIAS(rzi_at_uart)

#if !DT_NODE_EXISTS(AT_UART_NODE)
#error "The board overlay must define the rzi-at-uart alias"
#endif

static const struct device *const at_uart = DEVICE_DT_GET(AT_UART_NODE);

int main(void)
{
	if (!device_is_ready(at_uart)) {
		return -ENODEV;
	}

	/* Give a USB CDC host time to open the port before the service
	 * starts; on a plain UART this returns immediately.
	 */
	if (DT_NODE_HAS_COMPAT(AT_UART_NODE, zephyr_cdc_acm_uart)) {
		uint32_t dtr = 0;

		for (int i = 0; i < 100 && dtr == 0; i++) {
			(void)uart_line_ctrl_get(at_uart, UART_LINE_CTRL_DTR, &dtr);
			k_msleep(100);
		}
	}

	/* The AT service owns the UART from here on. */
	return rzi_at_uart_start(at_uart);
}
