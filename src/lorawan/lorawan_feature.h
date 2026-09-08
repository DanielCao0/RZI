/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Internal LoRaWAN backend feature-extension contract.
 */
#ifndef RZI_LORAWAN_FEATURE_H
#define RZI_LORAWAN_FEATURE_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Stable identifiers for optional backend feature contracts.
 *
 * Each implemented feature owns a private, typed operations structure. The
 * backend returns that structure through rzi_lorawan_backend_extension.
 */
enum rzi_lorawan_feature_id {
	RZI_LORAWAN_FEATURE_MULTICAST,
	RZI_LORAWAN_FEATURE_LINK_CHECK,
	RZI_LORAWAN_FEATURE_FUOTA,
	RZI_LORAWAN_FEATURE_PACKAGES,
	RZI_LORAWAN_FEATURE_NETWORK_MANAGEMENT,
	RZI_LORAWAN_FEATURE_CHANNEL_MANAGEMENT,
	RZI_LORAWAN_FEATURE_INFORMATION,
	RZI_LORAWAN_FEATURE_DEVICE_TIME,
	RZI_LORAWAN_FEATURE_CHANNEL_SCAN,
	RZI_LORAWAN_FEATURE_CERTIFICATION,
	RZI_LORAWAN_FEATURE_LONG_PACKET,
};

/**
 * @brief Versioned descriptor returned for an optional backend feature.
 *
 * size and version allow the service and backend to evolve independently.
 * api points to the typed private operations structure owned by the feature.
 */
struct rzi_lorawan_backend_extension {
	/** Size of the typed operations structure referenced by api. */
	size_t size;
	/** Feature contract version, beginning at one when implemented. */
	uint16_t version;
	/** Backend-owned typed operations structure. */
	const void *api;
};

/**
 * @brief Resolve and minimally validate one optional backend feature contract.
 *
 * @return Backend-owned extension descriptor, or NULL when unavailable or
 *         malformed.
 */
const struct rzi_lorawan_backend_extension *
rzi_lorawan_feature_get(enum rzi_lorawan_feature_id feature);

#endif /* RZI_LORAWAN_FEATURE_H */
