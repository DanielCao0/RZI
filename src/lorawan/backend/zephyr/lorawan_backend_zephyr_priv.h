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
const struct rzi_lorawan_backend_extension *
rzi_lorawan_zephyr_get_extension(enum rzi_lorawan_feature_id feature);

#endif /* RZI_LORAWAN_BACKEND_ZEPHYR_PRIV_H */
