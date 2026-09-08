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
#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup rzi_lorawan RZI LoRaWAN service
 *  @brief Thread-safe C API for the RZI LoRaWAN service.
 *  @since 0.2
 *  @version 0.2.0
 *  @{
 */

/** Maximum storage reserved for a LoRaWAN application payload. */
#define RZI_LORAWAN_MAX_PAYLOAD             242
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
	/** Zero on normal completion, otherwise a negative errno value. */
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
	/** Called with zero after join success or negative errno after failure. */
	void (*join_done)(int status, void *user_data);
	/** Called after an accepted uplink reaches a terminal result. */
	void (*send_done)(const struct rzi_lorawan_tx_result *result, void *user_data);
	/** Called for each application downlink. */
	void (*downlink)(const struct rzi_lorawan_downlink *downlink, void *user_data);
	/** Called for observable service state transitions. */
	void (*state_changed)(enum rzi_lorawan_state state, void *user_data);
	/** Called for asynchronous errors not attached to another result. */
	void (*error)(int error, void *user_data);
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
 * @retval -EINVAL A required pointer is NULL or the callback table is empty.
 * @retval -ENOMEM All callback subscriber slots are occupied.
 * @retval -EWOULDBLOCK Called from an ISR.
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
 * @retval -EINVAL The handle is invalid.
 * @retval -ENOENT The handle is not registered.
 * @retval -EWOULDBLOCK Called from an ISR.
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
 * @retval -EINVAL The region value is invalid.
 * @retval -EALREADY The service has already started.
 * @retval -EWOULDBLOCK Called from an ISR.
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
 * @retval -EALREADY The service has already started.
 * @retval -EWOULDBLOCK Called from an ISR.
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
 * @retval -EALREADY The service has already started.
 * @retval -EIO Backend startup failed.
 * @retval -EWOULDBLOCK Called from an ISR.
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
 * @retval -EINVAL The configuration is NULL or contains an invalid mode.
 * @retval -EAGAIN The service or backend is not ready.
 * @retval -EBUSY The backend is processing a conflicting operation.
 * @retval -EIO The backend operation failed.
 * @retval -ENOTSUP The selected backend does not support the activation mode.
 * @retval -EWOULDBLOCK Called from an ISR.
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
 * @retval -EAGAIN The service or backend is not ready.
 * @retval -EBUSY An uplink is still outstanding.
 * @retval -EINVAL The backend rejected the current session state.
 * @retval -ENOTSUP The selected backend cannot leave a session.
 * @retval -EIO The backend operation failed.
 * @retval -EWOULDBLOCK Called from an ISR.
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
 * @retval -EINVAL The port, payload, size, or message type is invalid.
 * @retval -EAGAIN The service or backend is not ready.
 * @retval -EBUSY Another uplink is outstanding.
 * @retval -EIO The backend operation failed.
 * @retval -EWOULDBLOCK Called from an ISR.
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
 * @retval -EINVAL The class value is invalid.
 * @retval -EAGAIN The service or backend is not ready.
 * @retval -EIO The backend operation failed.
 * @retval -ENOTSUP The selected backend does not support the class.
 * @retval -EWOULDBLOCK Called from an ISR.
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
 * @retval -EINVAL joined is NULL.
 * @retval -EAGAIN The service or backend is not ready.
 * @retval -EIO The backend operation failed.
 * @retval -EWOULDBLOCK Called from an ISR.
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

/** @} */

#ifdef __cplusplus
}
#endif
#endif
