/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RZI_AT_UART_H
#define RZI_AT_UART_H

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Start the AT service and take exclusive ownership of an interrupt UART. */
int rzi_at_uart_start(const struct device *uart);

#ifdef __cplusplus
}
#endif

#endif /* RZI_AT_UART_H */
