/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI product power-policy coordinator.
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
 *  @brief Sleep policy and blockers; Zephyr performs the transition.
 *
 *  The backend owns LoRaWAN RX1/RX2 timing. This service does not compute
 *  or delay receive windows. @ref rzi_power_enter selects the transition
 *  from the current policy. Retention sleep is System ON on nRF and STOP
 *  on STM32WLE5. Shutdown is System OFF on nRF and STANDBY on STM32WLE5.
 *
 *  @since 0.2
 *  @version 0.4.0
 *  @{
 */

/** How deeply the idle thread may sleep when no blocker is held. */
enum rzi_power_policy {
	/** Stay fully awake; lock every Zephyr PM state. */
	RZI_POWER_POLICY_RUN = 0,
	/** CPU idle only. nRF52840 System ON; STM32WLE5 WFI, not STOP. */
	RZI_POWER_POLICY_IDLE,
	/** Deepest RAM-retained sleep. STOP2 on STM32WLE5; idle on nRF52840. */
	RZI_POWER_POLICY_SUSPEND,
	/** RAM-lost shutdown via @ref rzi_power_shutdown. */
	RZI_POWER_POLICY_SHUTDOWN,
};

/** Service or application reason that forbids deep sleep or shutdown. */
enum rzi_power_blocker {
	/** Product code holds the device awake. */
	RZI_POWER_BLOCK_APP = 0,
	/** Class C continuous RX. Class A join/send do not take this. */
	RZI_POWER_BLOCK_LORAWAN,
	/** Flash write, settings store, or FUOTA image copy. */
	RZI_POWER_BLOCK_FLASH,
	/** UART or USB session that must not drop clocks. */
	RZI_POWER_BLOCK_TRANSPORT,
	/** Dedicated firmware-update session owned by the application. */
	RZI_POWER_BLOCK_UPDATE,
	/** Number of blocker identities (not a valid blocker). */
	RZI_POWER_BLOCK_COUNT,
};

/** Reset or wake cause captured once at @ref rzi_power_init. */
enum rzi_power_wake_reason {
	/** Cause unavailable or not classified. */
	RZI_POWER_WAKE_REASON_UNKNOWN = 0,
	/** Power-on or brownout reset. */
	RZI_POWER_WAKE_REASON_POWER_ON,
	/** Reset pin or other external pin. */
	RZI_POWER_WAKE_REASON_PIN,
	/** Software or bootloader reset. */
	RZI_POWER_WAKE_REASON_SOFTWARE,
	/** Watchdog reset. */
	RZI_POWER_WAKE_REASON_WATCHDOG,
	/** Wake from a RAM-lost low-power state. */
	RZI_POWER_WAKE_REASON_LOW_POWER,
	/** Debug or lock-up reset. */
	RZI_POWER_WAKE_REASON_DEBUG,
};

/** RTC or kernel timer may wake the CPU. */
#define RZI_POWER_WAKE_TIMER BIT(0)
/** GPIO sense / WKUP pin. Required for nRF52840 System OFF. */
#define RZI_POWER_WAKE_GPIO  BIT(1)
/** UART RX or line-control wake. */
#define RZI_POWER_WAKE_UART  BIT(2)
/** USB VBUS or resume. nRF52840 only in practice. */
#define RZI_POWER_WAKE_USB   BIT(3)
/** Radio IRQ (SX1262 DIO1 or STM32WL SUBGHZ EXTI). */
#define RZI_POWER_WAKE_RADIO BIT(4)

/**
 * @brief Initialize the coordinator and capture the reset cause.
 *
 * Idempotent. Later calls refresh Zephyr PM locks from the current policy
 * and blockers.
 *
 * @retval 0 Coordinator is ready.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_init(void);

/**
 * @brief Select the product sleep policy.
 *
 * @param policy Requested policy.
 *
 * @retval 0 Policy stored and PM locks updated.
 * @retval -RZI_ERR_INVALID @p policy is out of range.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_set_policy(enum rzi_power_policy policy);

/**
 * @brief Copy the current sleep policy.
 *
 * @param[out] policy Destination.
 *
 * @retval 0 Policy copied.
 * @retval -RZI_ERR_INVALID @p policy is NULL.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_get_policy(enum rzi_power_policy *policy);

/**
 * @brief Take one blocker reference.
 *
 * The same blocker may be taken more than once. Deep sleep is forbidden
 * while any reference remains. Calls @ref rzi_power_init if needed.
 *
 * @param blocker Blocker identity.
 *
 * @retval 0 Reference taken.
 * @retval -RZI_ERR_INVALID @p blocker is invalid.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_block(enum rzi_power_blocker blocker);

/**
 * @brief Release one blocker reference.
 *
 * @param blocker Blocker identity previously passed to @ref rzi_power_block.
 *
 * @retval 0 Reference released.
 * @retval -RZI_ERR_INVALID @p blocker is invalid or has no matching take.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_unblock(enum rzi_power_blocker blocker);

/**
 * @brief Copy the set of blockers that currently have a non-zero count.
 *
 * @param[out] blockers Bitmask of @ref rzi_power_blocker values.
 *
 * @retval 0 Mask copied.
 * @retval -RZI_ERR_INVALID @p blockers is NULL.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_get_blockers(uint32_t *blockers);

/**
 * @brief Return whether RAM-retained sleep is currently allowed.
 *
 * True when the policy is idle or suspend and no blocker is held.
 *
 * @return True if the idle thread may enter the policy sleep state.
 * @note Thread context only.
 * @since 0.2
 */
bool rzi_power_is_sleep_allowed(void);

/**
 * @brief Record the next time the product must be awake.
 *
 * Pass a negative value to clear the deadline. When Zephyr PM is enabled
 * the deadline is registered as a policy event so STOP is not entered
 * across it. This is not a LoRaWAN RX-window calculator.
 *
 * @param uptime_ms Absolute @c k_uptime_get() deadline, or negative to clear.
 *
 * @retval 0 Deadline stored.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_set_wake_deadline(int64_t uptime_ms);

/**
 * @brief Copy the configured wake deadline.
 *
 * @param[out] uptime_ms Absolute deadline, or a negative value when none.
 *
 * @retval 0 Value copied.
 * @retval -RZI_ERR_INVALID @p uptime_ms is NULL.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_get_wake_deadline(int64_t *uptime_ms);

/**
 * @brief Replace the advisory wake-source mask.
 *
 * The application (or board) must still enable the corresponding pins
 * or peripherals. Shutdown refuses a timer-only mask on nRF52840.
 *
 * @param sources Bitwise OR of @c RZI_POWER_WAKE_* flags. Zero restores
 *                the SoC default mask.
 *
 * @retval 0 Mask stored.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_set_wake_sources(uint32_t sources);

/**
 * @brief Copy the advisory wake-source mask.
 *
 * @param[out] sources Destination.
 *
 * @retval 0 Mask copied.
 * @retval -RZI_ERR_INVALID @p sources is NULL.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_get_wake_sources(uint32_t *sources);

/**
 * @brief Copy the reset or wake cause captured at init.
 *
 * @param[out] reason Destination.
 *
 * @retval 0 Reason copied.
 * @retval -RZI_ERR_INVALID @p reason is NULL.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_get_wake_reason(enum rzi_power_wake_reason *reason);

/**
 * @brief Sleep in the current policy until @p timeout_ms or the wake deadline.
 *
 * Does not enter RAM-lost shutdown. Blockers only make the sleep
 * shallower; they do not fail this call.
 *
 * @param timeout_ms Maximum milliseconds to remain in the sleep path.
 *                   A negative value waits only on the wake deadline,
 *                   or forever when no deadline is set.
 *
 * @retval 0 The timeout or wake deadline elapsed.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_sleep(int32_t timeout_ms);

/**
 * @brief Enter the low-power state for this board.
 *
 * The current policy selects the depth. Every board uses this call;
 * the SoC backend owns the registers.
 *
 * `RUN` stays awake. `IDLE` and `SUSPEND` prepare RAM-retained sleep
 * and then wait as @ref rzi_power_sleep does. Blockers do not fail
 * that wait. `SHUTDOWN` is @ref rzi_power_shutdown and does not return
 * on success.
 *
 * Before System OFF or STANDBY, the backend finishes SPI, UART, and
 * QSPI transfers, disables those peripherals, disconnects their pins,
 * and waits until the high-frequency clock has stopped. An external
 * wake pin must already use level sense (`GPIO_INT_LEVEL_LOW` or
 * `GPIO_INT_LEVEL_HIGH`). System OFF does not wake on an edge. Pins
 * that do not already have level sense return to the reset state.
 * A companion SX126x described by devicetree is put to sleep first.
 *
 * @param timeout_ms Maximum wait for a retention sleep. Ignored when
 *                   the policy is shutdown.
 *
 * @retval 0 Retention wait finished.
 * @retval -RZI_ERR_BUSY Policy is run, or shutdown is blocked.
 * @retval -RZI_ERR_NOT_SUPPORTED Shutdown is unavailable, or the wake-source mask cannot
 *                  wake this SoC from power-off.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_power_enter(int32_t timeout_ms);

/**
 * @brief Enter board shutdown if the policy and blockers allow it.
 *
 * Same path as @ref rzi_power_enter when the policy is shutdown.
 * Does not return on success. nRF uses System OFF; STM32WLE5 uses
 * STANDBY. A GPIO (or USB on 4631, RTC/radio on 3372) wake source
 * must be enabled by the application before the call.
 *
 * @retval -RZI_ERR_BUSY A blocker is held or the policy is not shutdown.
 * @retval -RZI_ERR_NOT_SUPPORTED Shutdown is unavailable or the wake-source mask cannot
 *                  wake this SoC from power-off.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_power_shutdown(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_POWER_POWER_H */
