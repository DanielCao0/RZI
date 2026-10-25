/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI service PM locks and controls for Zephyr system PM.
 */
#ifndef RZI_POWER_POWER_H
#define RZI_POWER_POWER_H

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/toolchain.h>

#include <rzi/err.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup rzi_power RZI power service
 *  @brief Service PM locks and native Zephyr PM controls.
 *
 *  RZI retains the public API boundary and translates operations to Zephyr.
 *  State meanings follow native PM; Zephyr selects the system state from
 *  the SoC device tree and policy. RZI adds no state-selection policy.
 *  The LoRaWAN backend owns RX1/RX2 timing.
 *
 *  @since 0.2
 *  @version 0.4.0
 *  @{
 */

/** Service or application reason that locks system PM. */
enum rzi_power_lock {
	/** Application holds a system PM lock. Ordinary CPU idle is still allowed. */
	RZI_POWER_LOCK_APP = 0,
	/** Class C continuous RX. Class A join/send do not take this. */
	RZI_POWER_LOCK_LORAWAN,
	/** Flash write, settings store, or FUOTA image copy. */
	RZI_POWER_LOCK_FLASH,
	/** UART or USB session that must not drop clocks. */
	RZI_POWER_LOCK_TRANSPORT,
	/** Dedicated firmware-update session owned by the application. */
	RZI_POWER_LOCK_UPDATE,
	/** Number of lock identities (not a valid lock). */
	RZI_POWER_LOCK_COUNT,
};

/**
 * @brief Allow system PM or hold one native all-state PM lock.
 *
 * Disabling acquires one pm_policy_state_all_lock_get() reference; enabling
 * releases only that reference. Repeated calls are idempotent. PM locks
 * and locks owned by other code remain in effect. Ordinary CPU idle and
 * device runtime PM are unaffected.
 *
 * @param enabled True to release the runtime-switch lock, false to acquire it.
 *
 * @retval 0 Switch updated.
 * @retval -RZI_ERR_NOT_SUPPORTED CONFIG_PM is disabled.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 */
__must_check int rzi_power_set_pm_enabled(bool enabled);

/**
 * @brief Read the RZI runtime switch for native system PM.
 *
 * Returns false when CONFIG_PM is disabled or the runtime switch holds its
 * lock. True does not mean a particular PM state is currently available;
 * PM locks, native locks, device readiness and residency times still apply.
 *
 * @param[out] enabled Destination for the switch value.
 *
 * @retval 0 Value copied.
 * @retval -RZI_ERR_INVALID @p enabled is NULL.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 */
__must_check int rzi_power_get_pm_enabled(bool *enabled);

/**
 * @brief Acquire one PM lock reference.
 *
 * Each reference takes a native all-state PM lock.
 * The same lock may be taken more than once; every take needs a matching
 * release. Without CONFIG_PM, only reference counts are tracked. Ordinary
 * CPU idle is unaffected. This is a power-state constraint, not a mutex;
 * acquiring a reference does not wait for other owners to release theirs.
 *
 * @param lock PM lock identity.
 *
 * @retval 0 Reference taken.
 * @retval -RZI_ERR_INVALID @p lock is invalid.
 * @retval -RZI_ERR_NO_RESOURCE The lock's reference count is full.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_lock_get(enum rzi_power_lock lock);

/**
 * @brief Release one PM lock reference.
 *
 * @param lock PM lock identity previously passed to @ref rzi_power_lock_get.
 *
 * @retval 0 Reference released.
 * @retval -RZI_ERR_INVALID @p lock is invalid or has no matching take.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_lock_put(enum rzi_power_lock lock);

/**
 * @brief Copy the set of PM locks that currently have a non-zero count.
 *
 * @param[out] lock_mask Bitmask of @ref rzi_power_lock values.
 *
 * @retval 0 Mask copied.
 * @retval -RZI_ERR_INVALID @p lock_mask is NULL.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_get_lock_mask(uint32_t *lock_mask);

/**
 * @brief Block the calling thread through Zephyr k_sleep().
 *
 * System sleep follows the native policy when no runnable work remains.
 * k_wakeup() may end the wait early.
 *
 * @param timeout_ms Relative duration in milliseconds; negative waits forever.
 *                   The native kernel timeout range applies.
 *
 * @retval 0 The wait completed or was ended early.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 */
__must_check int rzi_power_sleep(int64_t timeout_ms);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_POWER_POWER_H */
