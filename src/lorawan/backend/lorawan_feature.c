/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN backend feature-extension resolver.
 */

#include "lorawan_backend.h"

const struct rzi_lorawan_backend_extension *
rzi_lorawan_feature_get(enum rzi_lorawan_feature_id feature)
{
	const struct rzi_lorawan_backend_extension *extension;

	if (rzi_lorawan_backend.get_extension == NULL) {
		return NULL;
	}
	extension = rzi_lorawan_backend.get_extension(feature);
	if (extension == NULL || extension->api == NULL || extension->size == 0U ||
	    extension->version == 0U) {
		return NULL;
	}
	return extension;
}

const void *rzi_lorawan_feature_ops(enum rzi_lorawan_feature_id feature, uint16_t version,
				    size_t size)
{
	const struct rzi_lorawan_backend_extension *extension;

	extension = rzi_lorawan_feature_get(feature);
	if (extension == NULL || extension->version != version || extension->size < size) {
		return NULL;
	}
	return extension->api;
}
