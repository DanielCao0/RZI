/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Backend operations for LoRaWAN certification mode.
 */
#ifndef RZI_LORAWAN_CERTIFICATION_OPS_H
#define RZI_LORAWAN_CERTIFICATION_OPS_H

#include <stdbool.h>

/** Optional certification operations. NULL members return -RZI_ERR_NOT_SUPPORTED. */
struct rzi_lorawan_certification_ops {
	int (*get_mode)(bool *enabled);
	int (*set_mode)(bool enabled);
	int (*get_port_enabled)(bool *enabled);
	int (*set_port_enabled)(bool enabled);
};

#endif /* RZI_LORAWAN_CERTIFICATION_OPS_H */
