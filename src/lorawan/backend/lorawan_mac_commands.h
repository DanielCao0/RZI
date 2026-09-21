/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Backend operations for LinkCheckReq and DeviceTimeReq.
 */
#ifndef RZI_LORAWAN_MAC_COMMANDS_H
#define RZI_LORAWAN_MAC_COMMANDS_H

#include <rzi/lorawan/lorawan.h>

/** Optional MAC-request operations. NULL members return -RZI_ERR_NOT_SUPPORTED. */
struct rzi_lorawan_mac_ops {
	int (*request_link_check)(void);
	int (*request_device_time)(void);
	int (*get_network_time)(struct rzi_lorawan_network_time *time);
};

#endif /* RZI_LORAWAN_MAC_COMMANDS_H */
