/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Backend operations for LoRaWAN session identity.
 */
#ifndef RZI_LORAWAN_SESSION_OPS_H
#define RZI_LORAWAN_SESSION_OPS_H

#include <stdint.h>

/** Optional session-identity operations. NULL members return -RZI_ERR_NOT_SUPPORTED. */
struct rzi_lorawan_session_ops {
	int (*get_net_id)(uint32_t *net_id);
	int (*get_dev_nonce)(uint16_t *dev_nonce);
	int (*set_dev_nonce)(uint16_t dev_nonce);
	/** OTAA DevAddr in host byte order, or NULL when the stack cannot provide it. */
	int (*get_dev_addr)(uint32_t *dev_addr);
	/** OTAA NwkSEncKey, or NULL when the stack cannot export it. */
	int (*get_nwk_skey)(uint8_t *key);
	/** OTAA AppSKey, or NULL when the stack cannot export it. */
	int (*get_app_skey)(uint8_t *key);
};

#endif /* RZI_LORAWAN_SESSION_OPS_H */
