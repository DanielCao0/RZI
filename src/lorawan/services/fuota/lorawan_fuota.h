/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Internal FUOTA coordination contract.
 */
#ifndef RZI_LORAWAN_FUOTA_INTERNAL_H
#define RZI_LORAWAN_FUOTA_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../lorawan_backend.h"

#define RZI_LORAWAN_FUOTA_OPS_VERSION 1U

/** Backend operations used by the FUOTA coordinator. */
struct rzi_lorawan_fuota_ops {
	/** Start ALCSync and optionally request MAC DeviceTime. */
	int (*start_clock_sync)(void);
	/** Return the reconstructed image size, or a negative errno. */
	int (*get_image_size)(size_t *size);
	/** Read reconstructed image bytes from backend storage. */
	int (*read_image)(uint32_t offset, uint8_t *buffer, size_t size);
	/** Request a cold reboot. Does not return on success. */
	int (*reboot)(void);
};

/**
 * @brief Consume one backend event from the LoRaWAN dispatcher.
 *
 * Safe when the FUOTA service has not started; unsupported backends are
 * ignored.
 */
void rzi_lorawan_fuota_on_backend_event(const struct rzi_lorawan_backend_event *event);

/** Return true when a reconstructed image is available for FMP queries. */
bool rzi_lorawan_fuota_has_image(void);

#endif /* RZI_LORAWAN_FUOTA_INTERNAL_H */
