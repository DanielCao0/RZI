/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI AT command service API.
 */
#ifndef RZI_AT_H
#define RZI_AT_H

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup rzi_at RZI AT command service
 *  @brief Basic RAK RUI3-compatible AT interface over UART.
 *
 *  The service is a consumer of the RZI LoRaWAN C API: it owns one
 *  interrupt-driven UART and translates RUI3-style commands into service
 *  calls. Implemented subset: AT, ATZ, AT+VER, AT+DEVEUI, AT+APPEUI,
 *  AT+APPKEY, AT+BAND, AT+NJM (OTAA only), AT+NJS, AT+CLASS (A only),
 *  AT+CFM, AT+CFS, AT+JOIN, AT+SEND and AT+RECV.
 *
 *  Credentials and the region live in RAM. Defaults come from the
 *  zephyr,user devicetree node when present; AT updates apply to the next
 *  AT+JOIN. Reboot restores the devicetree defaults.
 *  @{
 */

/**
 * @brief Start the AT command service on a UART.
 *
 * Takes ownership of @p uart: no console, shell or logging backend may use
 * the same device. The UART must support interrupt-driven operation.
 *
 * @param uart UART device used for the AT interface.
 * @return 0 on success, or a negative errno value.
 */
int rzi_at_init(const struct device *uart);

/** @} */

#ifdef __cplusplus
}
#endif

#endif
