/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Private helpers shared by the Zephyr LoRaWAN backend.
 */
#ifndef RZI_LORAWAN_BACKEND_ZEPHYR_PRIV_H
#define RZI_LORAWAN_BACKEND_ZEPHYR_PRIV_H

#include <stdbool.h>
#include <stdint.h>

#include "../lorawan_backend.h"

int rzi_lorawan_zephyr_lock_started(void);
void rzi_lorawan_zephyr_unlock(void);
bool rzi_lorawan_zephyr_busy(void);
enum rzi_lorawan_region rzi_lorawan_zephyr_region(void);
uint16_t rzi_lorawan_zephyr_dev_nonce(void);
void rzi_lorawan_zephyr_set_dev_nonce(uint16_t dev_nonce);
void rzi_lorawan_zephyr_set_class(enum rzi_lorawan_class device_class);
void rzi_lorawan_zephyr_publish(const struct rzi_lorawan_backend_event *event);

int zephyr_get_class(enum rzi_lorawan_class *device_class);
int zephyr_query_tx_possible(size_t size);
int zephyr_is_busy(bool *busy);

extern const struct rzi_lorawan_network_ops zephyr_network_ops;
extern const struct rzi_lorawan_channel_ops zephyr_channel_ops;
extern const struct rzi_lorawan_session_ops zephyr_session_ops;
extern const struct rzi_lorawan_mac_ops zephyr_mac_ops;

#endif /* RZI_LORAWAN_BACKEND_ZEPHYR_PRIV_H */
