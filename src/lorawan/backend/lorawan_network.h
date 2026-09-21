/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Backend operations for LoRaWAN MAC parameters.
 */
#ifndef RZI_LORAWAN_NETWORK_H
#define RZI_LORAWAN_NETWORK_H

#include <stdbool.h>
#include <stdint.h>

#include <rzi/lorawan/lorawan.h>

/** Optional MAC-parameter operations. NULL members return -RZI_ERR_NOT_SUPPORTED. */
struct rzi_lorawan_network_ops {
	int (*get_adr)(bool *enabled);
	int (*set_adr)(bool enabled);
	int (*get_data_rate)(enum rzi_lorawan_data_rate *data_rate);
	int (*set_data_rate)(enum rzi_lorawan_data_rate data_rate);
	int (*get_tx_power)(uint8_t *tx_power);
	int (*set_tx_power)(uint8_t tx_power);
	int (*get_duty_cycle)(bool *enabled);
	int (*set_duty_cycle)(bool enabled);
	int (*get_rx1_delay)(uint32_t *delay_ms);
	int (*set_rx1_delay)(uint32_t delay_ms);
	int (*get_rx2_delay)(uint32_t *delay_ms);
	int (*set_rx2_delay)(uint32_t delay_ms);
	int (*get_rx2_data_rate)(enum rzi_lorawan_data_rate *data_rate);
	int (*set_rx2_data_rate)(enum rzi_lorawan_data_rate data_rate);
	int (*get_rx2_frequency)(uint32_t *frequency_hz);
	int (*set_rx2_frequency)(uint32_t frequency_hz);
	int (*get_join_accept_delay1)(uint32_t *delay_ms);
	int (*set_join_accept_delay1)(uint32_t delay_ms);
	int (*get_join_accept_delay2)(uint32_t *delay_ms);
	int (*set_join_accept_delay2)(uint32_t delay_ms);
	int (*get_public_network)(bool *enabled);
	int (*set_public_network)(bool enabled);
	int (*get_lbt)(bool *enabled);
	int (*set_lbt)(bool enabled);
	int (*get_lbt_rssi)(int16_t *rssi_dbm);
	int (*set_lbt_rssi)(int16_t rssi_dbm);
	int (*get_lbt_scan_time)(uint32_t *time_ms);
	int (*set_lbt_scan_time)(uint32_t time_ms);
};

#endif /* RZI_LORAWAN_NETWORK_H */
