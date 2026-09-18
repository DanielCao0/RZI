/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Internal backend operations for LoRaWAN certification mode.
 */
#ifndef RZI_LORAWAN_CERTIFICATION_INTERNAL_H
#define RZI_LORAWAN_CERTIFICATION_INTERNAL_H

#include <stdbool.h>

#define RZI_LORAWAN_CERTIFICATION_OPS_VERSION 1U

/** Optional certification operations. */
struct rzi_lorawan_certification_ops {
	int (*get_mode)(bool *enabled);
	int (*set_mode)(bool enabled);
	int (*get_port_enabled)(bool *enabled);
	int (*set_port_enabled)(bool enabled);
};

#endif /* RZI_LORAWAN_CERTIFICATION_INTERNAL_H */
