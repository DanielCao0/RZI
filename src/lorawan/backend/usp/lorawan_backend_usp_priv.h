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
const struct rzi_lorawan_backend_extension *
rzi_lorawan_usp_get_extension(enum rzi_lorawan_feature_id feature);

#endif /* RZI_LORAWAN_BACKEND_USP_PRIV_H */
