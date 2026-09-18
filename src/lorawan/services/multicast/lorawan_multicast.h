/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Internal backend operations for LoRaWAN multicast.
 */
#ifndef RZI_LORAWAN_MULTICAST_INTERNAL_H
#define RZI_LORAWAN_MULTICAST_INTERNAL_H

#include <rzi/lorawan/multicast.h>

#define RZI_LORAWAN_MULTICAST_OPS_VERSION 1U

/** Optional multicast operations. */
struct rzi_lorawan_multicast_ops {
	int (*add)(const struct rzi_lorawan_multicast_session *session);
	int (*remove)(uint32_t dev_addr);
	int (*get_count)(size_t *count);
	int (*get)(size_t index, struct rzi_lorawan_multicast_session *session);
	int (*clear)(void);
};

#endif /* RZI_LORAWAN_MULTICAST_INTERNAL_H */
