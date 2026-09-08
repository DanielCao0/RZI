/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RZI_LORAWAN_BACKEND_H
#define RZI_LORAWAN_BACKEND_H

#include <rzi/lorawan.h>

enum rzi_lorawan_backend_event_type {
	RZI_LORAWAN_BACKEND_READY,
	RZI_LORAWAN_BACKEND_JOINED,
	RZI_LORAWAN_BACKEND_JOIN_FAILED,
	RZI_LORAWAN_BACKEND_TX_DONE,
	RZI_LORAWAN_BACKEND_DOWNLINK,
	RZI_LORAWAN_BACKEND_ERROR,
	RZI_LORAWAN_BACKEND_STATE_CHANGED,
};

struct rzi_lorawan_backend_event {
	enum rzi_lorawan_backend_event_type type;
	union {
		int error;
		enum rzi_lorawan_tx_status tx_status;
		enum rzi_lorawan_state state;
		struct {
			uint8_t port;
			uint8_t size;
			int16_t rssi_dbm;
			int8_t snr_quarter_db;
			uint32_t flags;
			uint8_t data[RZI_LORAWAN_MAX_PAYLOAD];
		} downlink;
	};
};

typedef void (*rzi_lorawan_event_sink_t)(const struct rzi_lorawan_backend_event *event);

struct rzi_lorawan_backend_api {
	uint32_t capabilities;
	int (*start)(enum rzi_lorawan_region region, bool join_backoff_bypass,
		     rzi_lorawan_event_sink_t event_sink);
	int (*join)(const struct rzi_lorawan_join_config *config);
	int (*leave)(void);
	int (*send)(uint8_t port, const uint8_t *data, size_t size,
		    enum rzi_lorawan_message_type type);
	int (*set_class)(enum rzi_lorawan_class device_class);
	int (*is_joined)(bool *joined);
};

extern const struct rzi_lorawan_backend_api rzi_lorawan_backend;

#endif
