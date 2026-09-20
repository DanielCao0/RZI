/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Interrupt-driven UART I/O adapter for the RZI AT service.
 */
#ifndef RZI_AT_UART_H
#define RZI_AT_UART_H

#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/toolchain.h>

#include <rzi/err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup rzi_at_uart RZI AT UART adapter
 * @ingroup rzi_at
 * @brief UART binding for the RZI AT service.
 * @since 0.2
 * @version 0.4.0
 * @{
 */

/**
 * @brief Start the AT service on an interrupt-driven UART.
 *
 * RZI takes exclusive ownership of the UART receive callback after this
 * function succeeds.
 *
 * @param uart Ready UART device with interrupt-driven API support.
 *
 * @return Zero when started, otherwise a negative RZI_ERR_* from device
 *         validation, the UART driver, or the AT service.
 *
 * @pre UART interrupt support is enabled.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_at_uart_start(const struct device *uart);

/**
 * @brief Read the current AT UART baud rate.
 *
 * @param[out] baud_rate Destination.
 *
 * @retval 0 Baud rate stored.
 * @retval -RZI_ERR_NO_DEVICE The UART adapter is not started.
 * @retval -RZI_ERR_INVALID baud_rate is NULL.
 * @since 0.3
 */
__must_check int rzi_at_uart_get_baud(uint32_t *baud_rate);

/**
 * @brief Set the AT UART baud rate.
 *
 * @param baud_rate Requested baud rate.
 *
 * @retval 0 Baud rate applied.
 * @retval -RZI_ERR_NO_DEVICE The UART adapter is not started.
 * @retval -RZI_ERR_INVALID baud_rate is zero.
 * @since 0.3
 */
__must_check int rzi_at_uart_set_baud(uint32_t baud_rate);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_AT_UART_H */
