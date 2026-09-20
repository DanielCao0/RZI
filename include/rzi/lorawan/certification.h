/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI LoRaWAN certification-mode API.
 */
#ifndef RZI_LORAWAN_CERTIFICATION_H
#define RZI_LORAWAN_CERTIFICATION_H

#include <stdbool.h>
#include <zephyr/toolchain.h>

#include <rzi/err.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @ingroup rzi_lorawan
 *  @{
 */

/**
 * @brief Read whether LoRaWAN certification mode is enabled.
 *
 * @param[out] enabled True when certification mode is running.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID enabled is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support certification mode.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_certification_mode(bool *enabled);

/**
 * @brief Enable or disable LoRaWAN certification mode.
 *
 * @param enabled True to enable certification mode.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support certification mode.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_certification_mode(bool enabled);

/**
 * @brief Read whether the LoRaWAN certification FPort is enabled.
 *
 * @param[out] enabled True when the certification port is processed.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID enabled is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_certification_port_enabled(bool *enabled);

/**
 * @brief Enable or disable the LoRaWAN certification FPort.
 *
 * @param enabled True to process the certification port.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_certification_port_enabled(bool enabled);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_LORAWAN_CERTIFICATION_H */
