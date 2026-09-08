/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI SDK service capability discovery.
 */
#ifndef RZI_CAPABILITIES_H
#define RZI_CAPABILITIES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup rzi_capabilities RZI SDK capabilities
 *  @brief Compile-time service availability exposed at runtime.
 *  @since 0.2
 *  @version 0.2.0
 *  @{
 */

/** Process-wide RZI service capability bits. */
enum rzi_capability {
	/** Backend-independent LoRaWAN service is compiled in. */
	RZI_CAP_LORAWAN = (1U << 0),
	/** Transport-independent AT service is compiled in. */
	RZI_CAP_AT = (1U << 1),
	/** RZI key-value storage service is compiled in. */
	RZI_CAP_STORAGE = (1U << 2),
};

/**
 * @brief Return services compiled into the current firmware.
 *
 * Feature and backend-specific LoRaWAN capabilities are returned separately by
 * rzi_lorawan_get_capabilities().
 *
 * @return Bitwise OR of enum rzi_capability values.
 * @since 0.2
 */
uint32_t rzi_get_capabilities(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_CAPABILITIES_H */
