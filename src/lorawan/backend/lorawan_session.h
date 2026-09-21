/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Backend operations for LoRaWAN session identity.
 */
#ifndef RZI_LORAWAN_SESSION_H
#define RZI_LORAWAN_SESSION_H

#include <stdint.h>

/** Optional session-identity operations. NULL members return -RZI_ERR_NOT_SUPPORTED. */
struct rzi_lorawan_session_ops {
	int (*get_net_id)(uint32_t *net_id);
	int (*get_dev_nonce)(uint16_t *dev_nonce);
	int (*set_dev_nonce)(uint16_t dev_nonce);
};

#endif /* RZI_LORAWAN_SESSION_H */
