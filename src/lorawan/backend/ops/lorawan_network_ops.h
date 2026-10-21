/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Backend operations for LoRaWAN MAC parameters and link control.
 */
#ifndef RZI_LORAWAN_NETWORK_OPS_H
#define RZI_LORAWAN_NETWORK_OPS_H

#include <stdbool.h>
#include <stdint.h>

#include <rzi/lorawan/lorawan.h>

/**
 * Optional MAC-parameter operations, shaped after the LoRa Basics Modem API.
 * NULL members return -RZI_ERR_NOT_SUPPORTED.
 */
struct rzi_lorawan_network_ops {
	int (*get_adr)(bool *enabled);
	int (*set_adr)(bool enabled);
	/** Data rate of the next uplink. */
	int (*get_data_rate)(enum rzi_lorawan_data_rate *data_rate);
	int (*set_data_rate)(enum rzi_lorawan_data_rate data_rate);
	/** Bit n set when uplink DRn is legal for the active region. */
	int (*get_enabled_data_rates)(uint16_t *mask);
	int (*get_adr_profile)(enum rzi_lorawan_adr_profile *profile);
	/** distribution has RZI_LORAWAN_DR_DISTRIBUTION_LENGTH entries for CUSTOM. */
	int (*set_adr_profile)(enum rzi_lorawan_adr_profile profile, const uint8_t *distribution);
	int (*get_nb_trans)(uint8_t *nb_trans);
	int (*set_nb_trans)(uint8_t nb_trans);
	int (*get_adr_ack)(uint8_t *limit, uint8_t *delay);
	int (*set_adr_ack)(uint8_t limit, uint8_t delay);
	int (*set_join_data_rates)(const uint8_t *distribution);
	int (*get_duty_cycle_status)(int32_t *time_ms);
	int (*get_public_network)(bool *enabled);
	int (*set_public_network)(bool enabled);
	int (*get_lbt)(bool *enabled);
	int (*set_lbt)(bool enabled);
	int (*get_lbt_rssi)(int16_t *rssi_dbm);
	int (*set_lbt_rssi)(int16_t rssi_dbm);
	int (*get_lbt_scan_time)(uint32_t *time_ms);
	int (*set_lbt_scan_time)(uint32_t time_ms);
	int (*get_csma)(bool *enabled);
	int (*set_csma)(bool enabled);
	int (*get_csma_params)(struct rzi_lorawan_csma_params *params);
	int (*set_csma_params)(const struct rzi_lorawan_csma_params *params);
	int (*get_lost_connection_count)(uint16_t *count);
	int (*get_no_downlink_threshold)(uint16_t *uplinks);
	int (*set_no_downlink_threshold)(uint16_t uplinks);
	int (*get_crystal_error)(uint32_t *ppm);
	int (*set_crystal_error)(uint32_t ppm);
	/** Uplink without payload; completes through TX_DONE like send(). */
	int (*send_empty)(bool with_port, uint8_t port, enum rzi_lorawan_message_type type);
	/** Return whether the next uplink of this size can be sent, or NULL. */
	int (*query_tx_possible)(size_t size);
};

#endif /* RZI_LORAWAN_NETWORK_OPS_H */
