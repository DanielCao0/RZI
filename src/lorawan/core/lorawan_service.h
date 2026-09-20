/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Optional LoRaWAN service attachment points.
 */
#ifndef RZI_LORAWAN_SERVICE_H
#define RZI_LORAWAN_SERVICE_H

#include "../backend/lorawan_backend.h"

/**
 * @brief Hooks implemented by an optional LoRaWAN service.
 *
 * Core fans these out after start and on every dispatched event. It does not
 * name individual services such as FUOTA.
 */
struct rzi_lorawan_service {
	/** Called after the backend has started successfully. */
	void (*on_started)(void);
	/** Called for every backend event on the dispatcher thread. */
	void (*on_event)(const struct rzi_lorawan_backend_event *event);
};

/**
 * @brief Attach one optional LoRaWAN service.
 *
 * @param service Static-lifetime hook table.
 * @return Zero on success, otherwise a negative RZI_ERR_* value.
 */
int rzi_lorawan_register_service(const struct rzi_lorawan_service *service);

#endif /* RZI_LORAWAN_SERVICE_H */
