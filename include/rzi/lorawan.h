/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI LoRaWAN service API.
 */
#ifndef RZI_LORAWAN_H
#define RZI_LORAWAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup rzi_lorawan RZI LoRaWAN service
 *  @brief Thread-safe C API for the RZI LoRaWAN service.
 *  @{
 */

/** Maximum storage reserved for a LoRaWAN application payload. */
#define RZI_LORAWAN_MAX_PAYLOAD 242

/** LoRaWAN regional parameter selection. */
enum rzi_lorawan_region {
	RZI_LORAWAN_REGION_EU_868,
	RZI_LORAWAN_REGION_US_915,
	RZI_LORAWAN_REGION_AU_915,
	RZI_LORAWAN_REGION_CN_470,
	RZI_LORAWAN_REGION_AS_923_GRP1,
	RZI_LORAWAN_REGION_AS_923_GRP2,
	RZI_LORAWAN_REGION_AS_923_GRP3,
	RZI_LORAWAN_REGION_AS_923_GRP4,
	RZI_LORAWAN_REGION_IN_865,
	RZI_LORAWAN_REGION_KR_920,
	RZI_LORAWAN_REGION_RU_864,
};

/** LoRaWAN service initialization parameters. */
struct rzi_lorawan_config {
	enum rzi_lorawan_region region;
	uint8_t dev_eui[8];
	uint8_t join_eui[8];
	/* LBM network root key: LoRaWAN 1.0.x AppKey / 1.1 NwkKey. */
	uint8_t network_key[16];
	/* LBM application root key (1.1 AppKey / backend service key). */
	uint8_t application_key[16];
	/* Explicit development option, false by default. */
	bool join_backoff_bypass;
};

/** Events delivered by @ref rzi_lorawan_get_event. */
enum rzi_lorawan_event_type {
	RZI_LORAWAN_READY,
	RZI_LORAWAN_JOINED,
	RZI_LORAWAN_JOIN_FAILED,
	RZI_LORAWAN_TX_DONE,
	RZI_LORAWAN_DOWNLINK,
	RZI_LORAWAN_ERROR,
};

/** Result of a completed uplink request. */
enum rzi_lorawan_tx_result {
	RZI_LORAWAN_TX_SENT,
	RZI_LORAWAN_TX_ACKED,
	RZI_LORAWAN_TX_NOT_SENT,
};

/** Event copied from the RZI service to the application. */
struct rzi_lorawan_event {
	enum rzi_lorawan_event_type type;
	union {
		int error; /* Negative errno. */
		enum rzi_lorawan_tx_result tx_result;
		struct {
			uint8_t port;
			uint8_t size;
			int16_t rssi_dbm;
			int8_t snr_quarter_db;
			uint8_t data[RZI_LORAWAN_MAX_PAYLOAD];
		} downlink;
	};
};

/* Thread context only. APIs return 0 or negative errno. No heap allocation.
 * One modem instance and one event consumer. No teardown/reinit in v0.1.
 * init copies config; wait for READY before join/send. Init failure after
 * backend startup requires reboot. An unexpected modem reset re-emits READY.
 */
/**
 * @brief Initialize the process-wide LoRaWAN service.
 *
 * @param config Configuration copied by the service before returning.
 * @return 0 on success, or a negative errno value.
 */
int rzi_lorawan_init(const struct rzi_lorawan_config *config);

/** Request OTAA network activation. */
int rzi_lorawan_join(void);

/** Leave the current network session. */
int rzi_lorawan_leave(void);
/* 0 means accepted, not transmitted. LBM copies data before return.
 * One outstanding TX: a second send returns -EBUSY until TX_DONE.
 */
/**
 * @brief Request an application uplink.
 *
 * @param port LoRaWAN application port in the range 1 through 223.
 * @param data Payload copied by the backend before returning.
 * @param size Number of payload bytes.
 * @param confirmed Request a confirmed uplink when true.
 * @return 0 when accepted, or a negative errno value.
 */
int rzi_lorawan_send(uint8_t port, const uint8_t *data, size_t size, bool confirmed);

/** Query whether stack 0 currently has an active network session. */
int rzi_lorawan_is_joined(bool *joined);
/* Copies an event to caller-owned memory. timeout_ms: -1 forever, 0 poll,
 * positive values wait. -EAGAIN means no event; -EOVERFLOW reports dropped
 * events once, preserving queued events. Caller must reconcile or reboot.
 * Events are received in application context, never callbacks under USP locks.
 */
/**
 * @brief Wait for the next service event.
 *
 * @param event Destination for the copied event.
 * @param timeout_ms Negative one to wait forever, zero to poll, or a positive
 *        timeout in milliseconds.
 * @return 0 on success, -EAGAIN on timeout, or another negative errno value.
 */
int rzi_lorawan_get_event(struct rzi_lorawan_event *event, int32_t timeout_ms);

/** @} */

#ifdef __cplusplus
}
#endif
#endif
