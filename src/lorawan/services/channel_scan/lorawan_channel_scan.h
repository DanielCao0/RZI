/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Internal backend operations for LoRaWAN channel scanning.
 */
#ifndef RZI_LORAWAN_CHANNEL_SCAN_INTERNAL_H
#define RZI_LORAWAN_CHANNEL_SCAN_INTERNAL_H

#include <rzi/lorawan/channel_scan.h>

#define RZI_LORAWAN_CHANNEL_SCAN_OPS_VERSION 1U

/** Optional channel-scan operations. */
struct rzi_lorawan_channel_scan_ops {
	int (*get_count)(size_t *count);
	int (*get)(size_t index, struct rzi_lorawan_channel_rssi *rssi);
};

#endif /* RZI_LORAWAN_CHANNEL_SCAN_INTERNAL_H */
