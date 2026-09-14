/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Deprecated include; use <rzi/rsup/rsup.h>.
 */
#ifndef RZI_RSUP_SLOT_UPDATE_H
#define RZI_RSUP_SLOT_UPDATE_H

#include <rzi/rsup/rsup.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @deprecated Use @ref RZI_RSUP_GPREGRET. */
#define RZI_SLOT_UPDATE_GPREGRET RZI_RSUP_GPREGRET
/** @deprecated Use @ref RZI_RSUP_GPREGRET. */
#define RZI_USB_UPDATE_GPREGRET  RZI_RSUP_GPREGRET

/** @deprecated Use @ref RSUP_MAGIC. */
#define RZI1_MAGIC       RSUP_MAGIC
/** @deprecated Use @ref RSUP_TYPE_SKETCH. */
#define RZI1_TYPE_SKETCH RSUP_TYPE_SKETCH
/** @deprecated Use @ref RSUP_TYPE_SLOT1. */
#define RZI1_TYPE_SLOT1  RSUP_TYPE_SLOT1

/** @deprecated Use @ref rzi_rsup_uart. */
static inline const struct device *rzi_slot_update_uart(void)
{
	return rzi_rsup_uart();
}

/** @deprecated Use @ref rzi_rsup_requested. */
static inline bool rzi_slot_update_requested(void)
{
	return rzi_rsup_requested();
}

/** @deprecated Use @ref rzi_rsup_run. */
static inline int rzi_slot_update_run(const struct device *uart)
{
	return rzi_rsup_run(uart);
}

/** @deprecated Use @ref rzi_rsup_arm_reboot. */
static inline void rzi_slot_update_arm_reboot(void)
{
	rzi_rsup_arm_reboot();
}

/** @deprecated Use @ref rzi_rsup_requested. */
static inline bool rzi_usb_update_requested(void)
{
	return rzi_rsup_requested();
}

/** @deprecated Use @ref rzi_rsup_run. */
static inline int rzi_usb_update_run(const struct device *uart)
{
	return rzi_rsup_run(uart);
}

/** @deprecated Use @ref rzi_rsup_arm_reboot. */
static inline void rzi_usb_update_arm_reboot(void)
{
	rzi_rsup_arm_reboot();
}

#ifdef __cplusplus
}
#endif

#endif /* RZI_RSUP_SLOT_UPDATE_H */
