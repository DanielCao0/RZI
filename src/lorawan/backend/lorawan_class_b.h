/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Backend operations for LoRaWAN Class B.
 */
#ifndef RZI_LORAWAN_CLASS_B_H
#define RZI_LORAWAN_CLASS_B_H

#include <stdint.h>

#include <rzi/lorawan/lorawan.h>

/** Optional Class B operations. NULL members return -RZI_ERR_NOT_SUPPORTED. */
struct rzi_lorawan_class_b_ops {
	int (*get_ping_slot_periodicity)(uint8_t *periodicity);
	int (*set_ping_slot_periodicity)(uint8_t periodicity);
	int (*get_beacon_frequency)(uint32_t *frequency_hz);
	int (*get_beacon_time)(uint32_t *gps_time);
	int (*get_beacon_data_rate)(enum rzi_lorawan_data_rate *data_rate);
	int (*get_beacon_gateway)(struct rzi_lorawan_beacon_gateway *gateway);
	int (*get_state)(enum rzi_lorawan_class_b_state *state);
	int (*stop)(void);
};

#endif /* RZI_LORAWAN_CLASS_B_H */
