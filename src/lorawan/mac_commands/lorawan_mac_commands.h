/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Internal backend operations for DeviceTimeReq and LinkCheckReq.
 */
#ifndef RZI_LORAWAN_MAC_COMMANDS_INTERNAL_H
#define RZI_LORAWAN_MAC_COMMANDS_INTERNAL_H

#include <stdbool.h>

#include <rzi/lorawan/mac_commands.h>

#define RZI_LORAWAN_LINK_CHECK_OPS_VERSION  1U
#define RZI_LORAWAN_DEVICE_TIME_OPS_VERSION 1U

/** Optional LinkCheckReq operations. */
struct rzi_lorawan_link_check_ops {
	int (*request)(void);
};

/** Optional DeviceTimeReq operations. */
struct rzi_lorawan_device_time_ops {
	int (*request)(void);
	int (*get_network_time)(struct rzi_lorawan_network_time *time);
};

void rzi_lorawan_mac_commands_on_uplink(void);

#endif /* RZI_LORAWAN_MAC_COMMANDS_INTERNAL_H */
