/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Private helpers shared by the USP LoRaWAN backend.
 */
#ifndef RZI_LORAWAN_BACKEND_USP_PRIV_H
#define RZI_LORAWAN_BACKEND_USP_PRIV_H

#include <stdbool.h>

#include <smtc_modem_api.h>

#include "../lorawan_backend.h"

#define RZI_LORAWAN_USP_STACK_ID 0

int rzi_lorawan_usp_enter(void);
int rzi_lorawan_usp_finish(int rc);
int rzi_lorawan_usp_result(smtc_modem_return_code_t rc);
void rzi_lorawan_usp_publish(const struct rzi_lorawan_backend_event *event);
bool rzi_lorawan_usp_tx_pending(void);
void rzi_lorawan_usp_set_class_b_state(enum rzi_lorawan_class_b_state state);
enum rzi_lorawan_class_b_state rzi_lorawan_usp_class_b_state(void);

int usp_get_class(enum rzi_lorawan_class *device_class);
int usp_query_tx_possible(size_t size);
int usp_is_busy(bool *busy);

extern const struct rzi_lorawan_network_ops usp_network_ops;
extern const struct rzi_lorawan_class_b_ops usp_class_b_ops;
extern const struct rzi_lorawan_mac_ops usp_mac_ops;
extern const struct rzi_lorawan_multicast_ops usp_multicast_ops;
extern const struct rzi_lorawan_certification_ops usp_cert_ops;

#endif /* RZI_LORAWAN_BACKEND_USP_PRIV_H */
