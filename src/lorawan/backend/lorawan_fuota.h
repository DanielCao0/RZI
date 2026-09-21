/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Backend operations for LoRaWAN FUOTA image access.
 */
#ifndef RZI_LORAWAN_BACKEND_FUOTA_H
#define RZI_LORAWAN_BACKEND_FUOTA_H

#include <stddef.h>
#include <stdint.h>

/** Optional FUOTA operations. NULL members return -RZI_ERR_NOT_SUPPORTED. */
struct rzi_lorawan_fuota_ops {
	/** Start ALCSync and optionally request MAC DeviceTime. */
	int (*start_clock_sync)(void);
	/** Return the reconstructed image size, or a negative RZI_ERR_*. */
	int (*get_image_size)(size_t *size);
	/** Read reconstructed image bytes from backend storage. */
	int (*read_image)(uint32_t offset, uint8_t *buffer, size_t size);
	/** Request a cold reboot. Does not return on success. */
	int (*reboot)(void);
};

#endif /* RZI_LORAWAN_BACKEND_FUOTA_H */
