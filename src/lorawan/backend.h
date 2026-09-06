/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RZI_LORAWAN_BACKEND_H
#define RZI_LORAWAN_BACKEND_H

#include <rzi/lorawan.h>

typedef void (*rzi_lorawan_event_sink_t)(const struct rzi_lorawan_event *event);

struct rzi_lorawan_backend_api {
	int (*init)(const struct rzi_lorawan_config *config,
		    rzi_lorawan_event_sink_t event_sink);
	int (*join)(void);
	int (*leave)(void);
	int (*send)(uint8_t port, const uint8_t *data, size_t size, bool confirmed);
	int (*is_joined)(bool *joined);
};

extern const struct rzi_lorawan_backend_api rzi_lorawan_backend;

#endif
