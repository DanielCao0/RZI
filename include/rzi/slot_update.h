/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI1 slot-update protocol over any Zephyr UART.
 */
#ifndef RZI_SLOT_UPDATE_H
#define RZI_SLOT_UPDATE_H

#include <stdbool.h>
#include <zephyr/device.h>
#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup rzi_slot_update RZI UART slot update
 *  @brief Same RZI1 framing as the Arduino loader, type 1 only.
 *
 *  The wire protocol is a byte stream on a Zephyr UART device. Bind it to
 *  USB CDC or a hardware UART with chosen @c rzi,slot-update-uart. Type 0
 *  (Arduino LLEXT sketch) is rejected here.
 *
 *  Enter dedicated update by writing @ref RZI_SLOT_UPDATE_GPREGRET and
 *  resetting. On CDC, a 1200-bps touch does that; on a plain UART the
 *  same baud change works if the driver reports line control.
 *
 *  @since 0.2
 *  @{
 */

/** Host 1200-bps touch writes this; the next boot consumes it. */
#define RZI_SLOT_UPDATE_GPREGRET 0xA5u

/** @deprecated Use @ref RZI_SLOT_UPDATE_GPREGRET. */
#define RZI_USB_UPDATE_GPREGRET RZI_SLOT_UPDATE_GPREGRET

/** Wire magic: ASCII "RZI1" (RZI slot-update protocol, version 1). */
#define RZI1_MAGIC "RZI1"

#define RZI1_TYPE_SKETCH 0u
#define RZI1_TYPE_SLOT1  1u

/**
 * @brief UART selected by @c rzi,slot-update-uart (else @c zephyr,console).
 *
 * @return Device pointer; check @c device_is_ready before use.
 * @since 0.2
 */
const struct device *rzi_slot_update_uart(void);

/**
 * @brief Consume a pending GPREGRET update request.
 *
 * @retval true Host asked for slot update (flag cleared).
 * @retval false No request.
 * @since 0.2
 */
bool rzi_slot_update_requested(void);

/**
 * @brief Blocking RZI1 update on @p uart until success, error, or timeout.
 *
 * CDC waits for DTR first. A hardware UART starts immediately. On a
 * successful type-1 write the device reboots and does not return.
 *
 * @param uart UART from @ref rzi_slot_update_uart, or any UART device.
 *
 * @retval 0 Host did not send a frame; caller may continue.
 * @retval -EINVAL Bad header or unsupported type.
 * @retval -ENODEV @p uart is missing or not ready.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_slot_update_run(const struct device *uart);

/**
 * @brief Arm GPREGRET and cold-reset so the next boot stays in update.
 *
 * @since 0.2
 */
void rzi_slot_update_arm_reboot(void);

/** @deprecated Use @ref rzi_slot_update_requested. */
static inline bool rzi_usb_update_requested(void)
{
	return rzi_slot_update_requested();
}

/** @deprecated Use @ref rzi_slot_update_run. */
static inline int rzi_usb_update_run(const struct device *uart)
{
	return rzi_slot_update_run(uart);
}

/** @deprecated Use @ref rzi_slot_update_arm_reboot. */
static inline void rzi_usb_update_arm_reboot(void)
{
	rzi_slot_update_arm_reboot();
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_SLOT_UPDATE_H */
