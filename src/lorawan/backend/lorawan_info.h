/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Backend operations for LoRaWAN session information.
 */
#ifndef RZI_LORAWAN_INFO_H
#define RZI_LORAWAN_INFO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RZI_LORAWAN_INFO_OPS_VERSION 1U

/** Optional information operations. NULL members return -RZI_ERR_NOT_SUPPORTED. */
struct rzi_lorawan_info_ops {
	int (*get_net_id)(uint32_t *net_id);
	int (*get_dev_nonce)(uint16_t *dev_nonce);
	int (*set_dev_nonce)(uint16_t dev_nonce);
	int (*query_tx_possible)(size_t size);
	int (*is_busy)(bool *busy);
};

#endif /* RZI_LORAWAN_INFO_H */
