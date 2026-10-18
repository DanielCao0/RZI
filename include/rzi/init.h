/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Unified initialization for compiled-in RZI services.
 */
#ifndef RZI_INIT_H
#define RZI_INIT_H

#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup rzi_init RZI initialization
 *  @brief One entry that prepares every compiled-in service with an init step.
 *  @since 0.4
 *  @version 0.4.0
 *  @{
 */

/**
 * @brief Initialize every compiled-in RZI service that has an init step.
 *
 * When enabled, this calls @ref rzi_power_init and then @ref rzi_storage_init.
 * Both are idempotent, and so is this function. LoRaWAN, raw LoRa, and AT
 * stay stopped: the application configures them and then calls their
 * `start` functions.
 *
 * @retval 0 Every compiled-in init step succeeded.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval negative The first service init error. Later steps are not called.
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_init(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_INIT_H */
