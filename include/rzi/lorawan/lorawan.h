/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI LoRaWAN service API.
 */
#ifndef RZI_LORAWAN_LORAWAN_H
#define RZI_LORAWAN_LORAWAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/toolchain.h>

#include <rzi/err.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup rzi_lorawan RZI LoRaWAN service
 *  @brief Thread-safe C API for the RZI LoRaWAN service.
 *  @since 0.2
 *  @version 0.4.0
 *  @{
 */

/** Maximum storage reserved for a LoRaWAN application payload. */
#define RZI_LORAWAN_MAX_PAYLOAD             242
/** Maximum channel-mask words for US915, AU915, and CN470. */
#define RZI_LORAWAN_CHANNEL_MASK_WORDS      6
/** Sentinel that never identifies a registered callback subscriber. */
#define RZI_LORAWAN_CALLBACK_HANDLE_INVALID 0U

/** LoRaWAN regional parameter selection. */
enum rzi_lorawan_region {
	/** Europe 863-870 MHz. */
	RZI_LORAWAN_REGION_EU_868,
	/** United States 902-928 MHz. */
	RZI_LORAWAN_REGION_US_915,
	/** Australia 915-928 MHz. */
	RZI_LORAWAN_REGION_AU_915,
	/** China 470-510 MHz. */
	RZI_LORAWAN_REGION_CN_470,
	/** AS923 group 1. */
	RZI_LORAWAN_REGION_AS_923_GRP1,
	/** AS923 group 2. */
	RZI_LORAWAN_REGION_AS_923_GRP2,
	/** AS923 group 3. */
	RZI_LORAWAN_REGION_AS_923_GRP3,
	/** AS923 group 4. */
	RZI_LORAWAN_REGION_AS_923_GRP4,
	/** India 865-867 MHz. */
	RZI_LORAWAN_REGION_IN_865,
	/** South Korea 920-923 MHz. */
	RZI_LORAWAN_REGION_KR_920,
	/** Russia 864-870 MHz. */
	RZI_LORAWAN_REGION_RU_864,
};

/** Network activation method. */
enum rzi_lorawan_activation {
	/** Over-the-air activation. */
	RZI_LORAWAN_ACTIVATION_OTAA,
	/** Activation by personalization. */
	RZI_LORAWAN_ACTIVATION_ABP,
};

/** Uplink confirmation policy. */
enum rzi_lorawan_message_type {
	/** Uplink does not request a network acknowledgment. */
	RZI_LORAWAN_MSG_UNCONFIRMED,
	/** Uplink requests a network acknowledgment. */
	RZI_LORAWAN_MSG_CONFIRMED,
};

/** Supported LoRaWAN device classes. */
enum rzi_lorawan_class {
	/** Class A operation. */
	RZI_LORAWAN_CLASS_A,
	/** Beacon-synchronized Class B operation. */
	RZI_LORAWAN_CLASS_B,
	/** Continuously listening Class C operation. */
	RZI_LORAWAN_CLASS_C,
};

/** LoRaWAN data-rate index. Region tables decide which values are valid. */
enum rzi_lorawan_data_rate {
	RZI_LORAWAN_DR_0 = 0,
	RZI_LORAWAN_DR_1,
	RZI_LORAWAN_DR_2,
	RZI_LORAWAN_DR_3,
	RZI_LORAWAN_DR_4,
	RZI_LORAWAN_DR_5,
	RZI_LORAWAN_DR_6,
	RZI_LORAWAN_DR_7,
	RZI_LORAWAN_DR_8,
	RZI_LORAWAN_DR_9,
	RZI_LORAWAN_DR_10,
	RZI_LORAWAN_DR_11,
	RZI_LORAWAN_DR_12,
	RZI_LORAWAN_DR_13,
	RZI_LORAWAN_DR_14,
	RZI_LORAWAN_DR_15,
};

/** Observable Class B acquisition states. */
enum rzi_lorawan_class_b_state {
	/** Class B is not running. */
	RZI_LORAWAN_CLASS_B_IDLE,
	/** Searching for a beacon. */
	RZI_LORAWAN_CLASS_B_ACQUIRING_BEACON,
	/** Beacon locked; ping-slot setup is in progress. */
	RZI_LORAWAN_CLASS_B_ACQUIRING_PING_SLOT,
	/** Switching the stack into Class B. */
	RZI_LORAWAN_CLASS_B_SWITCHING,
	/** Beacon-locked Class B operation. */
	RZI_LORAWAN_CLASS_B_ACTIVE,
};

/** Gateway coordinates carried by a Class B beacon. */
struct rzi_lorawan_beacon_gateway {
	/** True when latitude and longitude are present. */
	bool has_coordinates;
	/** Beacon latitude as reported by the gateway. */
	uint32_t latitude;
	/** Beacon longitude as reported by the gateway. */
	uint32_t longitude;
	/** Network identifier from the beacon. */
	uint32_t net_id;
	/** Gateway identifier from the beacon. */
	uint32_t gateway_id;
};

/** Result of a completed LinkCheckAns. */
struct rzi_lorawan_link_check_result {
	/** Demodulation margin in dB. */
	uint8_t demod_margin;
	/** Number of gateways that heard the request. */
	uint8_t gateway_count;
	/** RSSI of the associated downlink in dBm, or zero if unknown. */
	int16_t rssi_dbm;
	/** SNR of the associated downlink in quarter-dB units. */
	int8_t snr_quarter_db;
};

/** Backend capabilities returned by @ref rzi_lorawan_get_capabilities. */
enum rzi_lorawan_capability {
	/** Backend supports OTAA. */
	RZI_LORAWAN_CAP_OTAA = (1U << 0),
	/** Backend supports ABP. */
	RZI_LORAWAN_CAP_ABP = (1U << 1),
	/** Backend supports Class A. */
	RZI_LORAWAN_CAP_CLASS_A = (1U << 2),
	/** Backend supports Class B. */
	RZI_LORAWAN_CAP_CLASS_B = (1U << 3),
	/** Backend supports Class C. */
	RZI_LORAWAN_CAP_CLASS_C = (1U << 4),
	/** Backend supports multicast-session management. */
	RZI_LORAWAN_CAP_MULTICAST = (1U << 5),
	/** Backend supports LinkCheckReq operations. */
	RZI_LORAWAN_CAP_LINK_CHECK = (1U << 6),
	/** Backend supports the complete RZI FUOTA coordination contract. */
	RZI_LORAWAN_CAP_FUOTA = (1U << 7),
	/** Backend supports session-information queries. */
	RZI_LORAWAN_CAP_INFORMATION = (1U << 8),
	/** Backend supports channel-plan management. */
	RZI_LORAWAN_CAP_CHANNEL_MANAGEMENT = (1U << 9),
	/** Backend supports network and MAC parameter management. */
	RZI_LORAWAN_CAP_NETWORK_MANAGEMENT = (1U << 10),
	/** Backend supports network-provided device time. */
	RZI_LORAWAN_CAP_DEVICE_TIME = (1U << 11),
	/** Backend supports channel scanning. */
	RZI_LORAWAN_CAP_CHANNEL_SCAN = (1U << 12),
	/** Backend supports certification mode. */
	RZI_LORAWAN_CAP_CERTIFICATION = (1U << 13),
};

/** OTAA credentials. They are copied before @ref rzi_lorawan_join returns. */
struct rzi_lorawan_join_otaa {
	/** Device EUI in network registration byte order. */
	uint8_t dev_eui[8];
	/** Join EUI in network registration byte order. */
	uint8_t join_eui[8];
	/** LoRaWAN 1.0.x AppKey or 1.1 NwkKey. */
	uint8_t network_key[16];
	/**
	 * LoRaWAN 1.0.x GenAppKey or 1.1 AppKey.
	 *
	 * USP/LBM uses this as GenAppKey. The Zephyr LoRaWAN API maps it to
	 * join app_key, so LoRaWAN 1.0.x applications must set both keys to
	 * AppKey unless the backend documents separate GenAppKey support.
	 */
	uint8_t application_key[16];
	/**
	 * Device nonce used by backends that do not persist DevNonce
	 * themselves.
	 *
	 * Zero means the backend should increment its own stored value.
	 * LoRaWAN 1.0.4+ requires a monotonically increasing nonce for the
	 * same DevEUI; the application should persist and supply it when the
	 * backend cannot.
	 */
	uint16_t dev_nonce;
};

/** ABP session parameters. A backend may report this mode as unsupported. */
struct rzi_lorawan_join_abp {
	/** LoRaWAN device address in host byte order. */
	uint32_t dev_addr;
	/** Network session key. */
	uint8_t network_session_key[16];
	/** Application session key. */
	uint8_t application_session_key[16];
};

/** Network activation parameters. */
struct rzi_lorawan_join_config {
	/** Selects the active union member. */
	enum rzi_lorawan_activation activation;
	union {
		/** Parameters used when activation is OTAA. */
		struct rzi_lorawan_join_otaa otaa;
		/** Parameters used when activation is ABP. */
		struct rzi_lorawan_join_abp abp;
	};
};

/** Observable service states delivered through state_changed. */
enum rzi_lorawan_state {
	/** Service has not started. */
	RZI_LORAWAN_STATE_STOPPED,
	/** Backend startup is in progress. */
	RZI_LORAWAN_STATE_STARTING,
	/** Backend is ready to accept network activation. */
	RZI_LORAWAN_STATE_READY,
	/** Network activation is in progress. */
	RZI_LORAWAN_STATE_JOINING,
	/** A network session is active. */
	RZI_LORAWAN_STATE_JOINED,
};

/** Result of a completed uplink request. */
enum rzi_lorawan_tx_status {
	/** Unconfirmed uplink was transmitted. */
	RZI_LORAWAN_TX_SENT,
	/** Confirmed uplink was acknowledged. */
	RZI_LORAWAN_TX_ACKED,
	/** Uplink did not complete successfully. */
	RZI_LORAWAN_TX_NOT_SENT,
};

/** Detailed result delivered when an asynchronous uplink completes. */
struct rzi_lorawan_tx_result {
	/** Protocol-level transmit result. */
	enum rzi_lorawan_tx_status status;
	/** Zero on normal completion, otherwise a negative RZI_ERR_* value. */
	int error;
};

/** Downlink metadata and payload. Data is valid only for the callback duration. */
struct rzi_lorawan_downlink {
	/** LoRaWAN application port. */
	uint8_t port;
	/** Number of payload bytes referenced by data. */
	size_t size;
	/** Received signal strength in dBm. */
	int16_t rssi_dbm;
	/** Signal-to-noise ratio in quarter-dB units. */
	int8_t snr_quarter_db;
	/** Backend-independent downlink flags. */
	uint32_t flags;
	/** Dispatcher-owned payload valid only during the callback. */
	const uint8_t *data;
};

/** Opaque callback subscriber registration handle. */
typedef uint16_t rzi_lorawan_callback_handle_t;

/**
 * @brief Asynchronous service callbacks.
 *
 * All callbacks are optional and execute serially in the RZI dispatcher
 * thread, never in an ISR or the backend modem callback. Implementations must
 * return promptly and must copy downlink data they need after returning.
 *
 * @note A callback may call non-blocking RZI LoRaWAN APIs.
 * @since 0.2
 */
struct rzi_lorawan_callbacks {
	/** Called with zero after join success or a negative RZI_ERR_* after failure. */
	void (*join_done)(int status, void *user_data);
	/** Called after an accepted uplink reaches a terminal result. */
	void (*send_done)(const struct rzi_lorawan_tx_result *result, void *user_data);
	/** Called for each application downlink. */
	void (*downlink)(const struct rzi_lorawan_downlink *downlink, void *user_data);
	/** Called for observable service state transitions. */
	void (*state_changed)(enum rzi_lorawan_state state, void *user_data);
	/** Called for asynchronous errors not attached to another result. */
	void (*error)(int error, void *user_data);
	/** Called after a LinkCheckAns, or not at all when the request failed. */
	void (*link_check_done)(const struct rzi_lorawan_link_check_result *result,
				void *user_data);
	/** Called with zero after DeviceTimeAns, otherwise a negative RZI_ERR_*. */
	void (*device_time_done)(int status, void *user_data);
	/** Opaque subscriber pointer passed to every callback. */
	void *user_data;
};

/**
 * @brief Register one callback subscriber.
 *
 * The callback table is copied. The caller owns the object referenced by
 * callbacks->user_data and must keep it valid until no callback is in flight
 * after unregistering.
 *
 * @param callbacks Callback table copied before this function returns.
 * @param[out] handle Registration handle used to unregister this subscriber.
 *
 * @retval 0 Subscriber registered.
 * @retval -RZI_ERR_INVALID A required pointer is NULL or the callback table is empty.
 * @retval -RZI_ERR_NO_RESOURCE All callback subscriber slots are occupied.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_lorawan_register_callbacks(const struct rzi_lorawan_callbacks *callbacks,
						rzi_lorawan_callback_handle_t *handle);

/**
 * @brief Unregister a callback subscriber.
 *
 * A callback already present in the dispatcher snapshot may still be in
 * flight when this function returns.
 *
 * @param handle Handle returned by rzi_lorawan_register_callbacks().
 *
 * @retval 0 Subscriber unregistered.
 * @retval -RZI_ERR_INVALID The handle is invalid.
 * @retval -RZI_ERR_NOT_FOUND The handle is not registered.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_lorawan_unregister_callbacks(rzi_lorawan_callback_handle_t handle);

/**
 * @brief Select the LoRaWAN region.
 *
 * @param region Region used by the selected backend.
 *
 * @retval 0 Region stored.
 * @retval -RZI_ERR_INVALID The region value is invalid.
 * @retval -RZI_ERR_ALREADY The service has already started.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @pre The service has not started.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_lorawan_set_region(enum rzi_lorawan_region region);

/**
 * @brief Configure the development-only join duty-cycle bypass.
 *
 * @param enabled True to bypass join backoff; false for standards-compliant
 *        behavior.
 *
 * @retval 0 Policy stored.
 * @retval -RZI_ERR_ALREADY The service has already started.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @warning Enabling this option may violate regional duty-cycle requirements
 *          and must not be used in production. Backends that have no join
 *          duty-cycle control ignore this setting.
 * @pre The service has not started.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_lorawan_set_join_backoff_bypass(bool enabled);

/**
 * @brief Start the process-wide LoRaWAN service.
 *
 * A return value of zero means startup was accepted. Readiness is reported
 * through state_changed() with RZI_LORAWAN_STATE_READY.
 *
 * @retval 0 Startup accepted.
 * @retval -RZI_ERR_ALREADY The service has already started.
 * @retval -RZI_ERR_IO Backend startup failed.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only. Runtime stop and reinitialization are not
 *       supported.
 * @since 0.2
 */
__must_check int rzi_lorawan_start(void);

/**
 * @brief Request OTAA or ABP network activation.
 *
 * The configuration is copied before this function returns. A return value of
 * zero means the request was accepted; completion is reported through
 * join_done().
 *
 * @param config Activation mode and credentials.
 *
 * @retval 0 Request accepted.
 * @retval -RZI_ERR_INVALID The configuration is NULL or contains an invalid mode.
 * @retval -RZI_ERR_NOT_READY The service or backend is not ready.
 * @retval -RZI_ERR_BUSY The backend is processing a conflicting operation.
 * @retval -RZI_ERR_IO The backend operation failed.
 * @retval -RZI_ERR_NOT_SUPPORTED The selected backend does not support the activation mode.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @pre RZI_LORAWAN_STATE_READY has been reported.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_lorawan_join(const struct rzi_lorawan_join_config *config);

/**
 * @brief Leave the current network session.
 *
 * @retval 0 Leave request accepted.
 * @retval -RZI_ERR_NOT_READY The service or backend is not ready.
 * @retval -RZI_ERR_BUSY An uplink is still outstanding.
 * @retval -RZI_ERR_INVALID The backend rejected the current session state.
 * @retval -RZI_ERR_NOT_SUPPORTED The selected backend cannot leave a session.
 * @retval -RZI_ERR_IO The backend operation failed.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_lorawan_leave(void);

/**
 * @brief Request an application uplink.
 *
 * @param port LoRaWAN application port in the range 1 through 223.
 * @param data Payload copied by the backend before returning.
 * @param size Number of payload bytes.
 * @param type Confirmed or unconfirmed message type.
 * A return value of zero means the payload was copied and accepted, not that
 * it has been transmitted. Completion is reported through send_done(). Only
 * one outstanding uplink is currently supported.
 *
 * @retval 0 Request accepted.
 * @retval -RZI_ERR_INVALID The port, payload pointer, or message type is invalid.
 * @retval -RZI_ERR_TOO_LARGE The payload exceeds RZI_LORAWAN_MAX_PAYLOAD or
 *         the current data rate.
 * @retval -RZI_ERR_NOT_READY The service or backend is not ready.
 * @retval -RZI_ERR_NOT_JOINED No network session is active.
 * @retval -RZI_ERR_BUSY Another uplink is outstanding.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend cannot send.
 * @retval -RZI_ERR_IO The backend operation failed.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only. The backend copies data before returning.
 * @since 0.2
 */
__must_check int rzi_lorawan_send(uint8_t port, const uint8_t *data, size_t size,
				  enum rzi_lorawan_message_type type);

/**
 * @brief Change the LoRaWAN device class.
 *
 * @param device_class Requested Class A, B, or C operation.
 *
 * @retval 0 Class selected.
 * @retval -RZI_ERR_INVALID The class value is invalid.
 * @retval -RZI_ERR_NOT_READY The service or backend is not ready.
 * @retval -RZI_ERR_IO The backend operation failed.
 * @retval -RZI_ERR_NOT_SUPPORTED The selected backend does not support the class.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_lorawan_set_class(enum rzi_lorawan_class device_class);

/**
 * @brief Query whether stack 0 has an active network session.
 *
 * @param[out] joined Set to true when the stack is joined.
 *
 * @retval 0 State returned.
 * @retval -RZI_ERR_INVALID joined is NULL.
 * @retval -RZI_ERR_NOT_READY The service or backend is not ready.
 * @retval -RZI_ERR_IO The backend operation failed.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_lorawan_is_joined(bool *joined);

/**
 * @brief Return the selected backend capability mask.
 *
 * @return Bitwise OR of values from enum rzi_lorawan_capability.
 * @since 0.2
 */
uint32_t rzi_lorawan_get_capabilities(void);

/**
 * @brief Return the region selected by rzi_lorawan_set_region().
 *
 * @param[out] region Stored region.
 *
 * @retval 0 Region stored.
 * @retval -RZI_ERR_INVALID region is NULL.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only. May be called before start.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_region(enum rzi_lorawan_region *region);

/**
 * @brief Return the currently requested device class.
 *
 * @param[out] device_class Stored class.
 *
 * @retval 0 Class stored.
 * @retval -RZI_ERR_INVALID device_class is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_class(enum rzi_lorawan_class *device_class);

/**
 * @brief Read whether ADR is enabled.
 *
 * @param[out] enabled True when ADR is enabled.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID enabled is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support network management.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_adr(bool *enabled);

/**
 * @brief Enable or disable ADR.
 *
 * @param enabled True to enable network-controlled ADR.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support network management.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_adr(bool enabled);

/**
 * @brief Read the current uplink data rate.
 *
 * @param[out] data_rate Stored data-rate index.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID data_rate is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_data_rate(enum rzi_lorawan_data_rate *data_rate);

/**
 * @brief Set the uplink data rate used when ADR is off.
 *
 * @param data_rate Data-rate index.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID The data-rate value is invalid.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_data_rate(enum rzi_lorawan_data_rate data_rate);

/**
 * @brief Read the LoRaWAN transmit-power index.
 *
 * @param[out] tx_power Stored power index.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID tx_power is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only. The value is a LoRaWAN power index, not dBm.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_tx_power(uint8_t *tx_power);

/**
 * @brief Set the LoRaWAN transmit-power index.
 *
 * @param tx_power Power index in the range 0 through 15.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID tx_power is out of range.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_tx_power(uint8_t tx_power);

/**
 * @brief Read whether duty-cycle limiting is enabled.
 *
 * @param[out] enabled True when duty-cycle limiting is enabled.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID enabled is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_duty_cycle(bool *enabled);

/**
 * @brief Enable or disable duty-cycle limiting.
 *
 * @param enabled True to enforce the regional duty cycle.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_duty_cycle(bool enabled);

/**
 * @brief Read RX1 delay in milliseconds.
 *
 * @param[out] delay_ms Stored delay.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID delay_ms is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_rx1_delay(uint32_t *delay_ms);

/**
 * @brief Set RX1 delay in milliseconds.
 *
 * @param delay_ms Receive-window delay.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID delay_ms is zero.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_rx1_delay(uint32_t delay_ms);

/**
 * @brief Read RX2 delay in milliseconds.
 *
 * @param[out] delay_ms Stored delay.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID delay_ms is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_rx2_delay(uint32_t *delay_ms);

/**
 * @brief Set RX2 delay in milliseconds.
 *
 * @param delay_ms Receive-window delay.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID delay_ms is zero.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_rx2_delay(uint32_t delay_ms);

/**
 * @brief Read the RX2 data rate.
 *
 * @param[out] data_rate Stored data-rate index.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID data_rate is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_rx2_data_rate(enum rzi_lorawan_data_rate *data_rate);

/**
 * @brief Set the RX2 data rate.
 *
 * @param data_rate Data-rate index.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID The data-rate value is invalid.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_rx2_data_rate(enum rzi_lorawan_data_rate data_rate);

/**
 * @brief Read the RX2 frequency in hertz.
 *
 * @param[out] frequency_hz Stored frequency.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID frequency_hz is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_rx2_frequency(uint32_t *frequency_hz);

/**
 * @brief Set the RX2 frequency in hertz.
 *
 * @param frequency_hz RX2 frequency.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID frequency_hz is zero.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_rx2_frequency(uint32_t frequency_hz);

/**
 * @brief Read Join-Accept delay 1 in milliseconds.
 *
 * @param[out] delay_ms Stored delay.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID delay_ms is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_join_accept_delay1(uint32_t *delay_ms);

/**
 * @brief Set Join-Accept delay 1 in milliseconds.
 *
 * @param delay_ms Join-accept window delay.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID delay_ms is zero.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_join_accept_delay1(uint32_t delay_ms);

/**
 * @brief Read Join-Accept delay 2 in milliseconds.
 *
 * @param[out] delay_ms Stored delay.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID delay_ms is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_join_accept_delay2(uint32_t *delay_ms);

/**
 * @brief Set Join-Accept delay 2 in milliseconds.
 *
 * @param delay_ms Join-accept window delay.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID delay_ms is zero.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_join_accept_delay2(uint32_t delay_ms);

/**
 * @brief Read whether public-network sync words are used.
 *
 * @param[out] enabled True for public network mode.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID enabled is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_public_network(bool *enabled);

/**
 * @brief Select public or private network mode.
 *
 * @param enabled True for public-network sync words.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_public_network(bool enabled);

/**
 * @brief Read whether LBT is enabled.
 *
 * @param[out] enabled True when listen-before-talk is enabled.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID enabled is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_lbt(bool *enabled);

/**
 * @brief Enable or disable LBT.
 *
 * @param enabled True to enable listen-before-talk.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_lbt(bool enabled);

/**
 * @brief Read the LBT RSSI threshold in dBm.
 *
 * @param[out] rssi_dbm Stored threshold.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID rssi_dbm is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_lbt_rssi(int16_t *rssi_dbm);

/**
 * @brief Set the LBT RSSI threshold in dBm.
 *
 * @param rssi_dbm Threshold used while listening.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_lbt_rssi(int16_t rssi_dbm);

/**
 * @brief Read the LBT listen duration in milliseconds.
 *
 * @param[out] time_ms Stored duration.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID time_ms is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_lbt_scan_time(uint32_t *time_ms);

/**
 * @brief Set the LBT listen duration in milliseconds.
 *
 * @param time_ms Listen duration.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID time_ms is zero.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_lbt_scan_time(uint32_t time_ms);

/**
 * @brief Read the current channel mask.
 *
 * @param[out] mask Buffer of @ref RZI_LORAWAN_CHANNEL_MASK_WORDS words.
 * @param words Number of 16-bit words in mask.
 *
 * @retval 0 Mask copied.
 * @retval -RZI_ERR_INVALID mask is NULL or words is zero.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support channel management.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_channel_mask(uint16_t *mask, size_t words);

/**
 * @brief Set the channel mask.
 *
 * @param mask Channel-mask words copied before return.
 * @param words Number of 16-bit words in mask.
 *
 * @retval 0 Mask applied.
 * @retval -RZI_ERR_INVALID mask is NULL or words is invalid for the region.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support channel management.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_channel_mask(const uint16_t *mask, size_t words);

/**
 * @brief Read the 8-channel sub-band selection.
 *
 * @param[out] sub_band 0 for all channels, or 1 through 8.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID sub_band is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The region or backend does not support sub-bands.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_sub_band(uint8_t *sub_band);

/**
 * @brief Select an 8-channel sub-band for US915, AU915, or CN470.
 *
 * @param sub_band 0 for all channels, or 1 through 8.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID sub_band is greater than 8.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The region or backend does not support sub-bands.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_sub_band(uint8_t sub_band);

/**
 * @brief Read the fixed-channel frequency in hertz.
 *
 * @param[out] frequency_hz Stored frequency, or zero when all channels are used.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID frequency_hz is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The region or backend does not support a fixed channel.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_fixed_channel(uint32_t *frequency_hz);

/**
 * @brief Restrict US915, AU915, or CN470 to one uplink channel.
 *
 * @param frequency_hz Channel frequency, or zero to restore the sub-band mask.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID The frequency is not a regional channel.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The region or backend does not support a fixed channel.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_fixed_channel(uint32_t frequency_hz);

/**
 * @brief Read RSSI of the last application downlink.
 *
 * @param[out] rssi_dbm Stored RSSI in dBm.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID rssi_dbm is NULL.
 * @retval -RZI_ERR_NO_DATA No downlink has been received.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_last_rssi(int16_t *rssi_dbm);

/**
 * @brief Read SNR of the last application downlink.
 *
 * @param[out] snr_quarter_db Stored SNR in quarter-dB units.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID snr_quarter_db is NULL.
 * @retval -RZI_ERR_NO_DATA No downlink has been received.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_last_snr(int8_t *snr_quarter_db);

/**
 * @brief Return the LoRaWAN specification string implemented by the stack.
 *
 * @param[out] version Pointer to a static NUL-terminated string.
 *
 * @retval 0 Pointer stored.
 * @retval -RZI_ERR_INVALID version is NULL.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only. The pointer remains valid for the process lifetime.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_protocol_version(const char **version);

/**
 * @brief Read the current NetID.
 *
 * @param[out] net_id Stored NetID.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID net_id is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_net_id(uint32_t *net_id);

/**
 * @brief Read the DevNonce that will be used for the next OTAA join.
 *
 * @param[out] dev_nonce Stored nonce.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID dev_nonce is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not expose DevNonce.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_dev_nonce(uint16_t *dev_nonce);

/**
 * @brief Set the DevNonce used by backends that do not persist it.
 *
 * @param dev_nonce Next OTAA nonce.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend manages DevNonce itself.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_dev_nonce(uint16_t dev_nonce);

/**
 * @brief Query whether an uplink of size bytes can be sent now.
 *
 * @param size Payload length to test.
 *
 * @retval 0 The payload fits the current data rate.
 * @retval -RZI_ERR_TOO_LARGE The payload is too large.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend cannot report payload limits.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_query_tx_possible(size_t size);

/**
 * @brief Query whether the stack is busy with radio work.
 *
 * @param[out] busy True when an uplink or join is outstanding.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID busy is NULL.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_is_busy(bool *busy);

/**
 * @brief Read Class B ping-slot periodicity.
 *
 * @param[out] periodicity Periodicity in the range 0 through 7.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID periodicity is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support Class B.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_ping_slot_periodicity(uint8_t *periodicity);

/**
 * @brief Set Class B ping-slot periodicity.
 *
 * @param periodicity Periodicity in the range 0 through 7.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID periodicity is greater than 7.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support Class B.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_ping_slot_periodicity(uint8_t periodicity);

/**
 * @brief Read the Class B beacon frequency in hertz.
 *
 * @param[out] frequency_hz Stored frequency.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID frequency_hz is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_beacon_frequency(uint32_t *frequency_hz);

/**
 * @brief Read the last beacon GPS time.
 *
 * @param[out] gps_time GPS seconds from the last beacon.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID gps_time is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NO_DATA No beacon has been received.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_beacon_time(uint32_t *gps_time);

/**
 * @brief Read the Class B beacon data rate.
 *
 * @param[out] data_rate Stored data-rate index.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID data_rate is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_beacon_data_rate(enum rzi_lorawan_data_rate *data_rate);

/**
 * @brief Read gateway information from the last Class B beacon.
 *
 * @param[out] gateway Stored beacon gateway fields.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID gateway is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NO_DATA No beacon gateway info is available.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_beacon_gateway(struct rzi_lorawan_beacon_gateway *gateway);

/**
 * @brief Read the Class B acquisition state.
 *
 * @param[out] state Stored Class B state.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID state is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support Class B.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_class_b_state(enum rzi_lorawan_class_b_state *state);

/**
 * @brief Stop Class B and return to Class A.
 *
 * @retval 0 Class B stopped.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support Class B.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_stop_class_b(void);

/** @} */

#ifdef __cplusplus
}
#endif
#endif
