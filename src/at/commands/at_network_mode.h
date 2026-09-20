/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Shared AT+NWM working-mode state.
 */
#ifndef RZI_AT_NETWORK_MODE_H
#define RZI_AT_NETWORK_MODE_H

#include <stdint.h>

/** P2P LoRa working mode. */
#define RZI_AT_NETWORK_MODE_P2P_LORA 0U
/** LoRaWAN working mode. */
#define RZI_AT_NETWORK_MODE_LORAWAN  1U
/** P2P FSK working mode. */
#define RZI_AT_NETWORK_MODE_P2P_FSK  2U

/** Notify a package after AT+NWM has been written. */
typedef void (*rzi_at_network_mode_changed_t)(uint8_t mode);

/** Return the current working mode. */
uint8_t rzi_at_network_mode_get(void);

/** Subscribe to working-mode changes. @p listener must have static lifetime. */
int rzi_at_network_mode_add_listener(rzi_at_network_mode_changed_t listener);

/** Register AT+NWM and load any persisted working mode. */
int rzi_at_network_mode_register(void);

#endif /* RZI_AT_NETWORK_MODE_H */
