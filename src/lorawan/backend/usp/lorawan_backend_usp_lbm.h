/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRa Basics Modem internals that the public modem API does not cover.
 *
 * The LBM headers declaring these pull in lr1mac and radio-planner internals,
 * so the prototypes are repeated here. lbm_checks.cmake checks each one
 * against the LBM sources, so a signature change fails the build.
 */
#ifndef RZI_LORAWAN_BACKEND_USP_LBM_H
#define RZI_LORAWAN_BACKEND_USP_LBM_H

#include <stdint.h>

#include <smtc_secure_element.h>

/* smtc_modem_core/lorawan_api/lorawan_api.h */
uint32_t lorawan_api_devaddr_get(uint8_t stack_id);
uint8_t lorawan_api_next_dr_get(uint8_t stack_id);
void lorawan_api_set_no_rx_packet_threshold(uint16_t no_rx_packet_reset_threshold,
					    uint8_t stack_id);
uint16_t lorawan_api_get_no_rx_packet_threshold(uint8_t stack_id);

#ifdef CONFIG_LORA_BASICS_MODEM_FUOTA_V2
/* lorawan_packages/fragmented_data_block_transport/lorawan_fragmentation_package.h */
void lorawan_fragmentation_package_get_file_size(uint8_t stack_id, uint32_t *file_size);
#endif

#endif /* RZI_LORAWAN_BACKEND_USP_LBM_H */
