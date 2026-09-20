/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RSUP (RZI Slot Update Protocol) over any Zephyr UART.
 */
#ifndef RZI_RSUP_RSUP_H
#define RZI_RSUP_RSUP_H

#include <stdbool.h>
#include <zephyr/device.h>
#include <zephyr/toolchain.h>

#include <rzi/err.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup rzi_rsup RSUP
 *  @brief Same RSUP framing as the Arduino loader, type 1 only.
 *
 *  The wire protocol is a byte stream on a Zephyr UART device. Bind it to
 *  USB CDC or a hardware UART with chosen @c rzi,rsup-uart. Type 0
 *  (Arduino LLEXT sketch) is rejected here.
 *
 *  Enter dedicated update by writing @ref RZI_RSUP_GPREGRET and
 *  resetting. On CDC, a 1200-bps touch does that; on a plain UART the
 *  same baud change works if the driver reports line control.
 *
 *  @since 0.2
 *  @version 0.4.0
 *  @{
 */

/** Host 1200-bps touch writes this; the next boot consumes it. */
#define RZI_RSUP_GPREGRET 0xA5u

/** Wire magic: ASCII "RSUP" (RZI Slot Update Protocol). */
#define RSUP_MAGIC "RSUP"

/** Arduino LLEXT sketch payload; RZI rejects this type. */
#define RSUP_TYPE_SKETCH 0u
/** Signed MCUboot slot1 image. */
#define RSUP_TYPE_SLOT1  1u

/**
 * @brief UART selected by @c rzi,rsup-uart (else @c zephyr,console).
 *
 * @return Device pointer; check @c device_is_ready before use.
 * @since 0.2
 */
const struct device *rzi_rsup_uart(void);

/**
 * @brief Consume a pending GPREGRET update request.
 *
 * @retval true Host asked for an RSUP session (flag cleared).
 * @retval false No request.
 * @since 0.2
 */
bool rzi_rsup_requested(void);

/**
 * @brief Blocking RSUP update on @p uart until success, error, or timeout.
 *
 * CDC waits for DTR first. A hardware UART starts immediately. On a
 * successful type-1 write the device reboots and does not return.
 *
 * @param uart UART from @ref rzi_rsup_uart, or any UART device.
 *
 * @retval 0 Host did not send a frame; caller may continue.
 * @retval -RZI_ERR_INVALID Bad header or unsupported type.
 * @retval -RZI_ERR_NO_DEVICE @p uart is missing or not ready.
 * @retval -RZI_ERR_TIMEOUT The host stopped sending.
 * @retval -RZI_ERR_BAD_MESSAGE The image CRC does not match.
 * @retval -RZI_ERR_IO Flash or image-write failed.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_rsup_run(const struct device *uart);

/**
 * @brief Arm GPREGRET and cold-reset so the next boot stays in update.
 *
 * @since 0.2
 */
void rzi_rsup_arm_reboot(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_RSUP_RSUP_H */
