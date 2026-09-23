/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Backend operations for LoRaWAN multicast.
 */
#ifndef RZI_LORAWAN_MULTICAST_OPS_H
#define RZI_LORAWAN_MULTICAST_OPS_H

#include <rzi/lorawan/multicast.h>

/** Optional multicast operations. NULL members return -RZI_ERR_NOT_SUPPORTED. */
struct rzi_lorawan_multicast_ops {
	int (*add)(const struct rzi_lorawan_multicast_session *session);
	int (*remove)(uint32_t dev_addr);
	int (*get_count)(size_t *count);
	int (*get)(size_t index, struct rzi_lorawan_multicast_session *session);
	int (*clear)(void);
};

#endif /* RZI_LORAWAN_MULTICAST_OPS_H */
