/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Backend operations for LoRaWAN channel-plan management.
 */
#ifndef RZI_LORAWAN_CHANNEL_H
#define RZI_LORAWAN_CHANNEL_H

#include <stddef.h>
#include <stdint.h>

#define RZI_LORAWAN_CHANNEL_OPS_VERSION 1U

/** Optional channel-plan operations. NULL members return -RZI_ERR_NOT_SUPPORTED. */
struct rzi_lorawan_channel_ops {
	int (*get_channel_mask)(uint16_t *mask, size_t words);
	int (*set_channel_mask)(const uint16_t *mask, size_t words);
	int (*get_sub_band)(uint8_t *sub_band);
	int (*set_sub_band)(uint8_t sub_band);
	int (*get_fixed_channel)(uint32_t *frequency_hz);
	int (*set_fixed_channel)(uint32_t frequency_hz);
};

#endif /* RZI_LORAWAN_CHANNEL_H */
