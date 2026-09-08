/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Internal backend contract for the RZI LoRaWAN service.
 */
#ifndef RZI_LORAWAN_BACKEND_H
#define RZI_LORAWAN_BACKEND_H

#include <rzi/lorawan.h>

#include "lorawan_feature.h"

/** Events published by a backend and consumed by the service dispatcher. */
enum rzi_lorawan_backend_event_type {
	/** Backend startup completed. */
	RZI_LORAWAN_BACKEND_READY,
	/** Network activation succeeded. */
	RZI_LORAWAN_BACKEND_JOINED,
	/** Network activation failed. */
	RZI_LORAWAN_BACKEND_JOIN_FAILED,
	/** An accepted uplink completed. */
	RZI_LORAWAN_BACKEND_TX_DONE,
	/** An application downlink was received. */
	RZI_LORAWAN_BACKEND_DOWNLINK,
	/** A backend error occurred outside another completion event. */
	RZI_LORAWAN_BACKEND_ERROR,
	/** Backend explicitly reports a service state transition. */
	RZI_LORAWAN_BACKEND_STATE_CHANGED,
	/** Optional FUOTA session event consumed by the FUOTA service. */
	RZI_LORAWAN_BACKEND_FUOTA,
};

/** FUOTA session kind carried by RZI_LORAWAN_BACKEND_FUOTA. */
enum rzi_lorawan_backend_fuota_kind {
	/** Application-layer or MAC time has been synchronized. */
	RZI_LORAWAN_BACKEND_FUOTA_CLOCK_SYNCED,
	/** A multicast fragment session has started. */
	RZI_LORAWAN_BACKEND_FUOTA_SESSION_STARTED,
	/** A multicast session has ended. */
	RZI_LORAWAN_BACKEND_FUOTA_SESSION_ENDED,
	/** Fragment reconstruction finished. */
	RZI_LORAWAN_BACKEND_FUOTA_TRANSFER_DONE,
	/** Firmware Management Package requested a reboot. */
	RZI_LORAWAN_BACKEND_FUOTA_REBOOT_REQUESTED,
};

/**
 * @brief Value-copied event passed from a backend to the service.
 *
 * The event sink copies the complete object before it returns. A backend may
 * therefore allocate this object on its stack.
 */
struct rzi_lorawan_backend_event {
	/** Selects the active union member. */
	enum rzi_lorawan_backend_event_type type;
	union {
		/** Negative errno for ERROR and JOIN_FAILED. */
		int error;
		/** Uplink result for RZI_LORAWAN_BACKEND_TX_DONE. */
		enum rzi_lorawan_tx_status tx_status;
		/** State for RZI_LORAWAN_BACKEND_STATE_CHANGED. */
		enum rzi_lorawan_state state;
		/** Value-owned downlink metadata and payload. */
		struct {
			/** LoRaWAN application port. */
			uint8_t port;
			/** Number of valid bytes in data. */
			uint8_t size;
			/** Received signal strength in dBm. */
			int16_t rssi_dbm;
			/** Signal-to-noise ratio in quarter-dB units. */
			int8_t snr_quarter_db;
			/** Backend-independent downlink flags. */
			uint32_t flags;
			/** Payload copied into the service event queue. */
			uint8_t data[RZI_LORAWAN_MAX_PAYLOAD];
		} downlink;
		/** FUOTA session details for RZI_LORAWAN_BACKEND_FUOTA. */
		struct {
			/** Selects the FUOTA session kind. */
			enum rzi_lorawan_backend_fuota_kind kind;
			/** True when a transfer completed successfully. */
			bool successful;
			/** Reconstructed image size in bytes, or zero if unknown. */
			uint32_t image_size;
		} fuota;
	};
};

/**
 * @brief Publish one backend event to the RZI service.
 *
 * The sink is non-blocking and may be called from ISR or thread context. It
 * copies event synchronously and never retains the pointer.
 */
typedef void (*rzi_lorawan_event_sink_t)(const struct rzi_lorawan_backend_event *event);

/**
 * @brief Operations supplied by exactly one selected LoRaWAN backend.
 *
 * Operation arguments must be consumed or copied before returning. A zero
 * return from join() or send() means accepted; completion is asynchronous
 * through the event sink.
 */
struct rzi_lorawan_backend_api {
	/** Bitwise OR of enum rzi_lorawan_capability values. */
	uint32_t capabilities;
	/** Start the backend and retain the event sink for its lifetime. */
	int (*start)(enum rzi_lorawan_region region, bool join_backoff_bypass,
		     rzi_lorawan_event_sink_t event_sink);
	/** Request network activation using the supplied copied credentials. */
	int (*join)(const struct rzi_lorawan_join_config *config);
	/** Leave or cancel the active network session. */
	int (*leave)(void);
	/** Request an application uplink and copy data before returning. */
	int (*send)(uint8_t port, const uint8_t *data, size_t size,
		    enum rzi_lorawan_message_type type);
	/** Select a device class supported by capabilities. */
	int (*set_class)(enum rzi_lorawan_class device_class);
	/** Store the current network-session state in joined. */
	int (*is_joined)(bool *joined);
	/**
	 * Return a versioned optional feature contract, or NULL when the
	 * feature is unsupported.
	 */
	const struct rzi_lorawan_backend_extension *(*get_extension)(
		enum rzi_lorawan_feature_id feature);
};

/** Backend implementation selected by the RZI Kconfig choice. */
extern const struct rzi_lorawan_backend_api rzi_lorawan_backend;

#endif /* RZI_LORAWAN_BACKEND_H */
