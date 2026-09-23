/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Private helpers shared by LoRaWAN public implementations.
 */
#ifndef RZI_LORAWAN_PRIV_H
#define RZI_LORAWAN_PRIV_H

#include <stdbool.h>

#include <rzi/lorawan/lorawan.h>

int rzi_lorawan_check_thread(void);
int rzi_lorawan_check_started(void);
void rzi_lorawan_note_class(enum rzi_lorawan_class device_class);
void rzi_lorawan_mac_commands_on_uplink(void);
bool rzi_lorawan_uplink_data_rate_valid(enum rzi_lorawan_data_rate data_rate);
bool rzi_lorawan_rx_data_rate_valid(enum rzi_lorawan_data_rate data_rate);

#endif /* RZI_LORAWAN_PRIV_H */
