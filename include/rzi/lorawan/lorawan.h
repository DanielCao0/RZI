/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
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

/** @name Constants and types
 *  @{
 */

/** Maximum storage reserved for a LoRaWAN application payload. */
#define RZI_LORAWAN_MAX_PAYLOAD           242
/** Channel-mask words accepted by the channel API; US915 and AU915 use 5 (72 channels). */
#define RZI_LORAWAN_CHANNEL_MASK_WORDS    6
/** Argument to @ref rzi_lorawan_join that leaves the stored value unchanged. */
#define RZI_LORAWAN_JOIN_KEEP             (-1)
/** Minimum seconds between join attempts. */
#define RZI_LORAWAN_JOIN_INTERVAL_MIN     7
/** Maximum seconds between join attempts. */
#define RZI_LORAWAN_JOIN_INTERVAL_MAX     255
/** Join retry interval used until a caller stores another value. */
#define RZI_LORAWAN_JOIN_INTERVAL_DEFAULT 8

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

/**
 * LoRaWAN MAC data-rate index (4-bit field).
 *
 * These names are the protocol index, not a promise that every value is
 * legal. `rzi_lorawan_set_data_rate()` uses the current region's uplink
 * table;  * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
`rzi_lorawan_set_rx2_data_rate()` and multicast use the RX2 /
 * downlink table. Other indexes return `-RZI_ERR_INVALID`.
 */
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

/** LinkCheckReq scheduling policy. */
enum rzi_lorawan_link_check_mode {
	/** Do not append LinkCheckReq. */
	RZI_LORAWAN_LINK_CHECK_DISABLED,
	/** Append LinkCheckReq to the next uplink only. */
	RZI_LORAWAN_LINK_CHECK_ONCE,
	/** Append LinkCheckReq to every uplink. */
	RZI_LORAWAN_LINK_CHECK_EVERY_UPLINK,
};

/** Network time from DeviceTimeAns, GPS epoch. */
struct rzi_lorawan_network_time {
	/** Whole GPS seconds. */
	uint32_t gps_seconds;
	/** Fractional GPS seconds as reported by the stack. */
	uint32_t gps_subseconds;
};

/**
 * @brief Backend capabilities returned by @ref rzi_lorawan_get_capabilities.
 *
 * Join-mode and class bits describe what start/join/set_class accept. The
 * remaining bits match optional ops tables on the backend contract: a bit
 * is set only when the corresponding pointer is non-NULL.
 */
enum rzi_lorawan_capability {
	/** Backend accepts OTAA join. */
	RZI_LORAWAN_CAP_OTAA = (1U << 0),
	/** Backend accepts ABP join. */
	RZI_LORAWAN_CAP_ABP = (1U << 1),
	/** Backend accepts Class A. */
	RZI_LORAWAN_CAP_CLASS_A = (1U << 2),
	/** Backend accepts Class B. */
	RZI_LORAWAN_CAP_CLASS_B = (1U << 3),
	/** Backend accepts Class C. */
	RZI_LORAWAN_CAP_CLASS_C = (1U << 4),
	/** Backend provides the MAC-parameter table. */
	RZI_LORAWAN_CAP_NETWORK = (1U << 5),
	/** Backend applies the US915 / AU915 channel mask from the join configuration. */
	RZI_LORAWAN_CAP_CHANNEL = (1U << 6),
	/** Backend provides session-identity queries (NetID / DevNonce). */
	RZI_LORAWAN_CAP_SESSION = (1U << 7),
	/** Backend provides LinkCheckReq and DeviceTimeReq. */
	RZI_LORAWAN_CAP_MAC = (1U << 8),
	/** Backend provides multicast-session management. */
	RZI_LORAWAN_CAP_MULTICAST = (1U << 9),
	/** Backend provides certification mode. */
	RZI_LORAWAN_CAP_CERTIFICATION = (1U << 10),
	/** Backend provides the RZI FUOTA coordination contract. */
	RZI_LORAWAN_CAP_FUOTA = (1U << 11),
};

/** Observable service states carried by RZI_LORAWAN_EVENT_STATE_CHANGED. */
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

/** Receive window that carried a downlink. */
enum rzi_lorawan_rx_window {
	/** The backend does not report the window. */
	RZI_LORAWAN_RX_WINDOW_UNKNOWN,
	/** Class A first receive window. */
	RZI_LORAWAN_RX_WINDOW_RX1,
	/** Class A second receive window. */
	RZI_LORAWAN_RX_WINDOW_RX2,
	/** Class C continuous receive window. */
	RZI_LORAWAN_RX_WINDOW_RXC,
	/** Class B ping slot. */
	RZI_LORAWAN_RX_WINDOW_RXB,
};

/** The network has more downlink data pending (FPending bit). */
#define RZI_LORAWAN_DOWNLINK_FPENDING     (1U << 0)
/** The downlink updated the network time (DeviceTimeAns). */
#define RZI_LORAWAN_DOWNLINK_TIME_UPDATED (1U << 1)
/** The downlink was received on a multicast session; see multicast_group. */
#define RZI_LORAWAN_DOWNLINK_MULTICAST    (1U << 2)

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
	/** Bitwise OR of RZI_LORAWAN_DOWNLINK_* flags. */
	uint32_t flags;
	/** Receive window, or RZI_LORAWAN_RX_WINDOW_UNKNOWN. */
	enum rzi_lorawan_rx_window window;
	/** Multicast group index when flags has RZI_LORAWAN_DOWNLINK_MULTICAST. */
	uint8_t multicast_group;
	/** Downlink data rate, or RZI_LORAWAN_DR_0 when unknown. */
	enum rzi_lorawan_data_rate data_rate;
	/** Downlink frequency in hertz, or zero when unknown. */
	uint32_t frequency_hz;
	/** Dispatcher-owned payload valid only during the callback. */
	const uint8_t *data;
};

/** RZI_LORAWAN_EVENT_NO_DOWNLINK reasons. */
#define RZI_LORAWAN_NO_DOWNLINK_ADR_BACKOFF_END (1U << 0)
/** The threshold set by rzi_lorawan_set_no_downlink_threshold() was reached. */
#define RZI_LORAWAN_NO_DOWNLINK_USER_THRESHOLD  (1U << 1)

/**
 * @brief Events delivered to rzi_lorawan_callbacks.on_event.
 *
 * FUOTA session progress stays on the FUOTA callback table.
 */
enum rzi_lorawan_event_type {
	/** Backend startup completed. */
	RZI_LORAWAN_EVENT_READY,
	/** Network activation succeeded. */
	RZI_LORAWAN_EVENT_JOINED,
	/** Network activation failed. Active member is error. */
	RZI_LORAWAN_EVENT_JOIN_FAILED,
	/** An accepted uplink completed. Active member is tx. */
	RZI_LORAWAN_EVENT_TX_DONE,
	/** An application downlink was received. Active member is downlink. */
	RZI_LORAWAN_EVENT_DOWNLINK,
	/** An asynchronous error. Active member is error. */
	RZI_LORAWAN_EVENT_ERROR,
	/** Service state transition. Active member is state. */
	RZI_LORAWAN_EVENT_STATE_CHANGED,
	/** LinkCheckAns received. Active member is link_check. */
	RZI_LORAWAN_EVENT_LINK_CHECK,
	/** DeviceTimeAns completed. Active member is error. */
	RZI_LORAWAN_EVENT_DEVICE_TIME,
	/** Class B acquisition state changed. Active member is class_b. */
	RZI_LORAWAN_EVENT_CLASS_B,
	/**
	 * Regional duty cycle started or stopped delaying uplinks. Active member
	 * is duty_cycle_limited.
	 */
	RZI_LORAWAN_EVENT_DUTY_CYCLE,
	/**
	 * Uplinks went unanswered for too long. Active member is no_downlink,
	 * a bitwise OR of RZI_LORAWAN_NO_DOWNLINK_* reasons.
	 */
	RZI_LORAWAN_EVENT_NO_DOWNLINK,
};

/**
 * @brief One queued LoRaWAN event.
 *
 * type selects the active union member. Downlink data is valid only for the
 * duration of on_event().
 */
struct rzi_lorawan_event {
	/** Selects the active union member. */
	enum rzi_lorawan_event_type type;
	union {
		/** Negative RZI_ERR_* for JOIN_FAILED, ERROR, and DEVICE_TIME. */
		int error;
		/** Uplink result for RZI_LORAWAN_EVENT_TX_DONE. */
		struct rzi_lorawan_tx_result tx;
		/** State for RZI_LORAWAN_EVENT_STATE_CHANGED. */
		enum rzi_lorawan_state state;
		/** Downlink metadata for RZI_LORAWAN_EVENT_DOWNLINK. */
		struct rzi_lorawan_downlink downlink;
		/** Link-check result for RZI_LORAWAN_EVENT_LINK_CHECK. */
		struct rzi_lorawan_link_check_result link_check;
		/** Class B state for RZI_LORAWAN_EVENT_CLASS_B. */
		enum rzi_lorawan_class_b_state class_b;
		/** True while the regional duty cycle delays uplinks. */
		bool duty_cycle_limited;
		/** RZI_LORAWAN_NO_DOWNLINK_* reasons. */
		uint8_t no_downlink;
	};
};

/**
 * @brief Asynchronous service callback.
 *
 * on_event executes serially in the RZI dispatcher thread, never in an ISR
 * or the backend modem callback. Each queued public event produces one call.
 * Implementations must return promptly and must copy downlink data they need
 * after returning.
 *
 * @note A callback may call non-blocking RZI LoRaWAN APIs.
 * @since 0.2
 */
struct rzi_lorawan_callbacks {
	/** Called once for each delivered public event. */
	void (*on_event)(const struct rzi_lorawan_event *event, void *user_data);
	/** Opaque subscriber pointer passed to on_event. */
	void *user_data;
};

/** @} */

/** @name Lifecycle
 *  @{
 */

/**
 * @brief Register one callback subscriber.
 *
 * The callback table is copied. The caller owns the object referenced by
 * callbacks->user_data and must keep it valid until no callback is in flight
 * after unregistering. A subscriber is identified by its on_event function
 * and user_data pointer.
 *
 * @param callbacks Callback table copied before this function returns.
 *
 * @retval 0 Subscriber registered.
 * @retval -RZI_ERR_INVALID callbacks is NULL or the callback table is empty.
 * @retval -RZI_ERR_ALREADY This on_event and user_data pair is already registered.
 * @retval -RZI_ERR_NO_RESOURCE All callback subscriber slots are occupied.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_lorawan_register_callbacks(const struct rzi_lorawan_callbacks *callbacks);

/**
 * @brief Unregister one callback subscriber.
 *
 * The subscriber is the registration whose on_event and user_data match this
 * table. A callback already present in the dispatcher snapshot may still be
 * in flight when this function returns.
 *
 * @param callbacks Table identifying the subscriber.
 *
 * @retval 0 Subscriber unregistered.
 * @retval -RZI_ERR_INVALID callbacks is NULL or on_event is NULL.
 * @retval -RZI_ERR_NOT_FOUND No subscriber matches this table.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_lorawan_unregister_callbacks(const struct rzi_lorawan_callbacks *callbacks);

/**
 * @brief Select the LoRaWAN region.
 *
 * @param region Region used by the selected backend.
 *
 * @retval 0 Region stored.
 * @retval -RZI_ERR_INVALID The region value is invalid.
 * @retval -RZI_ERR_INVALID_STATE The service has already started.
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_INVALID_STATE The service has already started.
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * through on_event() with RZI_LORAWAN_EVENT_READY.
 *
 * @retval 0 Startup accepted.
 * @retval -RZI_ERR_ALREADY The service has already started.
 * @retval -RZI_ERR_IO Backend startup failed.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only. Runtime stop and reinitialization are not
 *       supported.
 * @since 0.2
 */
__must_check int rzi_lorawan_start(void);

/** @} */

/** @name Credentials
 *  @{
 */

/**
 * @brief Read the stored DevEUI.
 *
 * @param[out] eui Destination for the 8-byte identifier.
 * @param len Size of eui. Must be 8.
 *
 * @retval 0 Identifier copied.
 * @retval -RZI_ERR_INVALID eui is NULL or len is not 8.
 * @retval -RZI_ERR_NO_DATA No DevEUI has been stored.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO Stored credentials could not be loaded.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_dev_eui(uint8_t *eui, size_t len);

/**
 * @brief Store the DevEUI used by the next join.
 *
 * @param eui 8-byte identifier copied before this function returns.
 * @param len Size of eui. Must be 8.
 *
 * @retval 0 Identifier stored.
 * @retval -RZI_ERR_INVALID eui is NULL or len is not 8.
 * @retval -RZI_ERR_BUSY A join sequence is in progress.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO The identifier could not be persisted.
 *
 * @note Thread context only. The value is applied on the next join.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_dev_eui(const uint8_t *eui, size_t len);

/**
 * @brief Read the stored AppEUI.
 *
 * AppEUI is the RUI3 name for the LoRaWAN JoinEUI.
 *
 * @param[out] eui Destination for the 8-byte identifier.
 * @param len Size of eui. Must be 8.
 *
 * @retval 0 Identifier copied.
 * @retval -RZI_ERR_INVALID eui is NULL or len is not 8.
 * @retval -RZI_ERR_NO_DATA No AppEUI has been stored.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO Stored credentials could not be loaded.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_app_eui(uint8_t *eui, size_t len);

/**
 * @brief Store the AppEUI used by the next join.
 *
 * AppEUI is the RUI3 name for the LoRaWAN JoinEUI.
 *
 * @param eui 8-byte identifier copied before this function returns.
 * @param len Size of eui. Must be 8.
 *
 * @retval 0 Identifier stored.
 * @retval -RZI_ERR_INVALID eui is NULL or len is not 8.
 * @retval -RZI_ERR_BUSY A join sequence is in progress.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO The identifier could not be persisted.
 *
 * @note Thread context only. The value is applied on the next join.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_app_eui(const uint8_t *eui, size_t len);

/**
 * @brief Read the stored AppKey.
 *
 * @param[out] key Destination for the 16-byte key.
 * @param len Size of key. Must be 16.
 *
 * @retval 0 Key copied.
 * @retval -RZI_ERR_INVALID key is NULL or len is not 16.
 * @retval -RZI_ERR_NO_DATA No AppKey has been stored.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO Stored credentials could not be loaded.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_app_key(uint8_t *key, size_t len);

/**
 * @brief Store the AppKey used by the next join.
 *
 * The value is the LoRaWAN 1.0.x AppKey and the 1.1 NwkKey. When no GenAppKey
 * is stored, join also uses this value as the application key.
 *
 * @param key 16-byte key copied before this function returns.
 * @param len Size of key. Must be 16.
 *
 * @retval 0 Key stored.
 * @retval -RZI_ERR_INVALID key is NULL or len is not 16.
 * @retval -RZI_ERR_BUSY A join sequence is in progress.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO The key could not be persisted.
 *
 * @note Thread context only. The value is applied on the next join.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_app_key(const uint8_t *key, size_t len);

/**
 * @brief Read the stored GenAppKey.
 *
 * @param[out] key Destination for the 16-byte key.
 * @param len Size of key. Must be 16.
 *
 * @retval 0 Key copied.
 * @retval -RZI_ERR_INVALID key is NULL or len is not 16.
 * @retval -RZI_ERR_NO_DATA No GenAppKey has been stored.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO Stored credentials could not be loaded.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_gen_app_key(uint8_t *key, size_t len);

/**
 * @brief Store the GenAppKey used by the next OTAA join.
 *
 * USP/LBM uses this as GenAppKey. The Zephyr backend maps it to join app_key.
 * Leave it unset to reuse the AppKey.
 *
 * @param key 16-byte key copied before this function returns.
 * @param len Size of key. Must be 16.
 *
 * @retval 0 Key stored.
 * @retval -RZI_ERR_INVALID key is NULL or len is not 16.
 * @retval -RZI_ERR_BUSY A join sequence is in progress.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO The key could not be persisted.
 *
 * @note Thread context only. The value is applied on the next join.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_gen_app_key(const uint8_t *key, size_t len);

/**
 * @brief Read the device address.
 *
 * ABP returns the stored address. OTAA returns 0 until the device has
 * joined, then the address assigned by the network.
 *
 * @param[out] dev_addr Device address in host byte order.
 *
 * @retval 0 Address copied.
 * @retval -RZI_ERR_INVALID dev_addr is NULL.
 * @retval -RZI_ERR_NO_DATA ABP is selected and no address has been stored.
 * @retval -RZI_ERR_NOT_SUPPORTED OTAA is joined and this backend cannot read the address.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO The stack read failed, or stored credentials could not be loaded.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_dev_addr(uint32_t *dev_addr);

/**
 * @brief Store the ABP device address used by the next join.
 *
 * @param dev_addr Device address in host byte order.
 *
 * @retval 0 Address stored.
 * @retval -RZI_ERR_BUSY A join sequence is in progress.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO The address could not be persisted.
 *
 * @note Thread context only. The value is applied on the next ABP join.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_dev_addr(uint32_t dev_addr);

/**
 * @brief Read the network session key.
 *
 * ABP returns the stored NwkSKey. OTAA returns 16 zero bytes until the
 * device has joined, then NwkSEncKey from the active stack.
 *
 * @param[out] key Destination for the 16-byte key.
 * @param len Size of key. Must be 16.
 *
 * @retval 0 Key copied.
 * @retval -RZI_ERR_INVALID key is NULL or len is not 16.
 * @retval -RZI_ERR_NO_DATA ABP is selected and no key has been stored.
 * @retval -RZI_ERR_NOT_SUPPORTED OTAA is joined and this backend cannot export the key.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO The stack read failed, or stored credentials could not be loaded.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_nwk_skey(uint8_t *key, size_t len);

/**
 * @brief Store the ABP network session key used by the next join.
 *
 * @param key 16-byte key copied before this function returns.
 * @param len Size of key. Must be 16.
 *
 * @retval 0 Key stored.
 * @retval -RZI_ERR_INVALID key is NULL or len is not 16.
 * @retval -RZI_ERR_BUSY A join sequence is in progress.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO The key could not be persisted.
 *
 * @note Thread context only. The value is applied on the next ABP join.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_nwk_skey(const uint8_t *key, size_t len);

/**
 * @brief Read the application session key.
 *
 * ABP returns the stored AppSKey. OTAA returns 16 zero bytes until the
 * device has joined, then AppSKey from the active stack.
 *
 * @param[out] key Destination for the 16-byte key.
 * @param len Size of key. Must be 16.
 *
 * @retval 0 Key copied.
 * @retval -RZI_ERR_INVALID key is NULL or len is not 16.
 * @retval -RZI_ERR_NO_DATA ABP is selected and no key has been stored.
 * @retval -RZI_ERR_NOT_SUPPORTED OTAA is joined and this backend cannot export the key.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO The stack read failed, or stored credentials could not be loaded.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_app_skey(uint8_t *key, size_t len);

/**
 * @brief Store the ABP application session key used by the next join.
 *
 * @param key 16-byte key copied before this function returns.
 * @param len Size of key. Must be 16.
 *
 * @retval 0 Key stored.
 * @retval -RZI_ERR_INVALID key is NULL or len is not 16.
 * @retval -RZI_ERR_BUSY A join sequence is in progress.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO The key could not be persisted.
 *
 * @note Thread context only. The value is applied on the next ABP join.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_app_skey(const uint8_t *key, size_t len);

/** @} */

/** @name Join
 *  @{
 */

/**
 * @brief Read the stored activation method.
 *
 * @param[out] mode OTAA or ABP. OTAA until a caller stores another method.
 *
 * @retval 0 Mode copied.
 * @retval -RZI_ERR_INVALID mode is NULL.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO Stored credentials could not be loaded.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_activation(enum rzi_lorawan_activation *mode);

/**
 * @brief Store the activation method used by the next join.
 *
 * @param mode OTAA or ABP.
 *
 * @retval 0 Mode stored.
 * @retval -RZI_ERR_INVALID mode is not OTAA or ABP.
 * @retval -RZI_ERR_NOT_SUPPORTED The selected backend does not support mode.
 * @retval -RZI_ERR_BUSY A join sequence is in progress.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO The mode could not be persisted.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_activation(enum rzi_lorawan_activation mode);

/**
 * @brief Read whether a join sequence starts after the next boot.
 *
 * @param[out] enabled True when automatic join is stored.
 *
 * @retval 0 Value copied.
 * @retval -RZI_ERR_INVALID enabled is NULL.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO Stored join settings could not be loaded.
 *
 * @note Thread context only. A stored automatic join does not start the
 *       service and does not join by itself.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_auto_join(bool *enabled);

/**
 * @brief Read the stored delay between join attempts.
 *
 * @param[out] interval_s Delay in seconds.
 *
 * @retval 0 Value copied.
 * @retval -RZI_ERR_INVALID interval_s is NULL.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO Stored join settings could not be loaded.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_join_interval(uint8_t *interval_s);

/**
 * @brief Read how many join retries follow the first attempt.
 *
 * @param[out] attempts Retry count after the first attempt. Zero means one
 *                      attempt in total.
 *
 * @retval 0 Value copied.
 * @retval -RZI_ERR_INVALID attempts is NULL.
 * @retval -RZI_ERR_ISR Called from an ISR.
 * @retval -RZI_ERR_IO Stored join settings could not be loaded.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_join_attempts(uint8_t *attempts);

/**
 * @brief Start or stop a join sequence using the stored credentials.
 *
 * `start`, `auto_join`, `interval`, and `attempts` follow the RUI3
 * `service_lora_join()` arguments. @ref RZI_LORAWAN_JOIN_KEEP leaves that
 * stored value unchanged. `start` of 1 uses the stored NJM and credentials.
 * `start` of 0 stops the join sequence, including an attempt still waiting
 * for Join-Accept. It does not leave a joined session. The stack schedules
 * retries itself; the service counts failed attempts and stops the stack
 * when `attempts` is exhausted. USP spaces attempts with the LoRaWAN
 * regional join backoff and ignores `interval`.
 * A return value of zero means the request was accepted. Completion is
 * reported through on_event() as RZI_LORAWAN_EVENT_JOINED, or as
 * RZI_LORAWAN_EVENT_JOIN_FAILED after the configured attempts are exhausted.
 *
 * @param start 1 to join, 0 to cancel retries, or @ref RZI_LORAWAN_JOIN_KEEP.
 * @param auto_join 1 to join after the next boot, 0 to disable that, or
 *                  @ref RZI_LORAWAN_JOIN_KEEP.
 * @param interval Seconds between attempts on backends without their own
 *                 join backoff (ignored by USP), from
 *                 @ref RZI_LORAWAN_JOIN_INTERVAL_MIN to
 *                 @ref RZI_LORAWAN_JOIN_INTERVAL_MAX, or
 *                 @ref RZI_LORAWAN_JOIN_KEEP. The default is
 *                 @ref RZI_LORAWAN_JOIN_INTERVAL_DEFAULT.
 * @param attempts Retries after the first attempt, 0 through 255, or
 *                 @ref RZI_LORAWAN_JOIN_KEEP. The default is 0.
 *
 * @retval 0 Request accepted, or a stop/update completed.
 * @retval -RZI_ERR_INVALID An argument is outside its accepted range.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_BUSY A join sequence is already active.
 * @retval -RZI_ERR_NOT_SUPPORTED The stored join mode is not supported.
 * @retval -RZI_ERR_IO The backend rejected the attempt or settings could not
 *         be persisted.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @pre For `start` of 1, on_event() has reported RZI_LORAWAN_EVENT_READY.
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_join(int32_t start, int32_t auto_join, int32_t interval,
				  int32_t attempts);

/** @} */

/** @name Uplink
 *  @{
 */

/**
 * @brief Leave the current network session.
 *
 * @retval 0 Leave request accepted.
 * @retval -RZI_ERR_NOT_READY The service or backend is not ready.
 * @retval -RZI_ERR_BUSY An uplink is still outstanding.
 * @retval -RZI_ERR_INVALID The backend rejected the current session state.
 * @retval -RZI_ERR_NOT_SUPPORTED The selected backend cannot leave a session.
 * @retval -RZI_ERR_IO The backend operation failed.
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * it has been transmitted. Completion is reported through on_event() as
 * RZI_LORAWAN_EVENT_TX_DONE. Only one outstanding uplink is currently supported.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only. The backend copies data before returning.
 * @since 0.2
 */
__must_check int rzi_lorawan_send(uint8_t port, const uint8_t *data, size_t size,
				  enum rzi_lorawan_message_type type);

/** @} */

/** @name Link check and device time
 *  @{
 */

/**
 * @brief Read the current LinkCheckReq mode.
 *
 * @param[out] mode Stored mode.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID mode is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support LinkCheckReq.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_link_check_mode(enum rzi_lorawan_link_check_mode *mode);

/**
 * @brief Request LinkCheckReq according to mode.
 *
 * Completion is reported through on_event() as RZI_LORAWAN_EVENT_LINK_CHECK.
 * A return of zero means the mode was stored. ONCE and EVERY_UPLINK trigger
 * a request immediately;
 * EVERY_UPLINK also piggybacks on later uplinks.
 *
 * @param mode Scheduling policy.
 *
 * @retval 0 Mode stored.
 * @retval -RZI_ERR_INVALID mode is invalid.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support LinkCheckReq.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_request_link_check(enum rzi_lorawan_link_check_mode mode);

/**
 * @brief Read whether DeviceTimeReq is enabled.
 *
 * @param[out] enabled True when DeviceTimeReq is scheduled.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID enabled is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support DeviceTimeReq.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_device_time_enabled(bool *enabled);

/**
 * @brief Enable or disable DeviceTimeReq on subsequent uplinks.
 *
 * Completion is reported through on_event() as RZI_LORAWAN_EVENT_DEVICE_TIME.
 *
 * @param enabled True to request network time.
 *
 * @retval 0 Setting stored.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support DeviceTimeReq.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_request_device_time(bool enabled);

/**
 * @brief Read the last DeviceTimeAns.
 *
 * @param[out] time Stored GPS time.
 *
 * @retval 0 Time stored.
 * @retval -RZI_ERR_INVALID time is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started or the clock is unsynchronized.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support DeviceTimeReq.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_network_time(struct rzi_lorawan_network_time *time);

/** @} */

/** @name Class
 *  @{
 */

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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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

/** @name MAC parameters
 *  @{
 */

/**
 * @brief Return the region selected by rzi_lorawan_set_region().
 *
 * @param[out] region Stored region.
 *
 * @retval 0 Region stored.
 * @retval -RZI_ERR_INVALID region is NULL.
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_data_rate(enum rzi_lorawan_data_rate *data_rate);

/**
 * @brief Set the uplink data rate used when ADR is off.
 *
 * @param data_rate Uplink data-rate index legal for the current region.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID The data-rate value is invalid.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only. The value is a LoRaWAN power index, not dBm.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_rx2_data_rate(enum rzi_lorawan_data_rate *data_rate);

/**
 * @brief Set the RX2 data rate.
 *
 * @param data_rate RX2 data-rate index legal for the current region.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID The data-rate value is invalid.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @note Reserved for RUI3 compatibility. Every current backend returns
 *       -RZI_ERR_NOT_SUPPORTED: LoRa Basics Modem takes this parameter from
 *       the regional parameters and network MAC commands.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_lbt_scan_time(uint32_t time_ms);

/** @} */

/** @name Link control
 *
 * Uplink-strategy controls modeled on LoRa Basics Modem. Every function
 * returns -RZI_ERR_NOT_SUPPORTED when the backend lacks the control.
 *  @{
 */

/**
 * Number of slots in a custom ADR or join data-rate distribution. Each slot
 * holds a data rate (0 through 15); the stack picks the data rate of each
 * frame from the slots, so a data rate's share is the number of slots holding
 * it. Slots holding a data rate illegal for the region are ignored.
 */
#define RZI_LORAWAN_DR_DISTRIBUTION_LENGTH 16U

/** Uplink data-rate strategy. */
enum rzi_lorawan_adr_profile {
	/** The network controls data rate and NbTrans through ADR. */
	RZI_LORAWAN_ADR_NETWORK,
	/** Device-side strategy favoring range (low data rates). */
	RZI_LORAWAN_ADR_MOBILE_LONG_RANGE,
	/** Device-side strategy favoring battery life (high data rates). */
	RZI_LORAWAN_ADR_MOBILE_LOW_POWER,
	/** Device-side strategy from a user distribution. */
	RZI_LORAWAN_ADR_CUSTOM,
};

/** CSMA (channel activity detection before transmit) parameters. */
struct rzi_lorawan_csma_params {
	/** Channel changes allowed before giving up on a busy channel. */
	uint8_t max_channel_changes;
	/** Back off on the same channel before changing channel. */
	bool backoff_enabled;
	/** Maximum number of back-offs per channel. */
	uint8_t max_backoffs;
};

/**
 * @brief Read the uplink data-rate strategy.
 *
 * @param[out] profile Stored profile.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID profile is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support ADR profiles.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_adr_profile(enum rzi_lorawan_adr_profile *profile);

/**
 * @brief Select the uplink data-rate strategy.
 *
 * @param profile Strategy to apply.
 * @param distribution For RZI_LORAWAN_ADR_CUSTOM, RZI_LORAWAN_DR_DISTRIBUTION_LENGTH
 *        data-rate slots as described there. Ignored for the other profiles
 *        and may be NULL.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID The profile is unknown, or CUSTOM has a NULL
 *         distribution or no slot legal for the region.
 * @retval -RZI_ERR_NOT_JOINED No network session is active.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support ADR profiles.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only. rzi_lorawan_set_adr() and
 *       rzi_lorawan_set_data_rate() select the network profile and a custom
 *       profile pinned to one data rate respectively.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_adr_profile(enum rzi_lorawan_adr_profile profile,
					     const uint8_t *distribution);

/**
 * @brief Read the number of transmissions of each unconfirmed uplink.
 *
 * @param[out] nb_trans Stored NbTrans, 1 through 15.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID nb_trans is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_nb_trans(uint8_t *nb_trans);

/**
 * @brief Set the number of transmissions of each unconfirmed uplink.
 *
 * @param nb_trans NbTrans, 1 through 15.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID nb_trans is out of range or the ADR profile is
 *         network-controlled, where the network owns NbTrans.
 * @retval -RZI_ERR_NOT_JOINED No network session is active.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_nb_trans(uint8_t nb_trans);

/**
 * @brief Read the ADR back-off parameters.
 *
 * @param[out] limit ADR_ACK_LIMIT uplinks.
 * @param[out] delay ADR_ACK_DELAY uplinks.
 *
 * @retval 0 Values stored.
 * @retval -RZI_ERR_INVALID A pointer is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_adr_ack(uint8_t *limit, uint8_t *delay);

/**
 * @brief Set the ADR back-off parameters.
 *
 * @param limit ADR_ACK_LIMIT, 2 through 127 uplinks.
 * @param delay ADR_ACK_DELAY, 2 through 127 uplinks.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID A value is out of range.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_adr_ack(uint8_t limit, uint8_t delay);

/**
 * @brief Set the data-rate distribution used by join requests.
 *
 * @param distribution RZI_LORAWAN_DR_DISTRIBUTION_LENGTH data-rate slots, as
 *        for rzi_lorawan_set_adr_profile().
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID distribution is NULL or illegal for the region.
 * @retval -RZI_ERR_BUSY A join is in progress or a session is active.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_join_data_rates(const uint8_t *distribution);

/**
 * @brief Read the regional duty-cycle budget.
 *
 * @param[out] time_ms Positive: transmit time still available. Negative:
 *             time to wait before the next uplink is allowed.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID time_ms is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not track duty cycle.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only. RZI_LORAWAN_EVENT_DUTY_CYCLE reports changes.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_duty_cycle_status(int32_t *time_ms);

/**
 * @brief Read whether CSMA is enabled.
 *
 * @param[out] enabled True when CSMA is enabled.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID enabled is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend or build lacks CSMA.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_csma(bool *enabled);

/**
 * @brief Enable or disable CSMA.
 *
 * @param enabled True to run channel activity detection before each uplink.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend or build lacks CSMA.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_csma(bool enabled);

/**
 * @brief Read the CSMA parameters.
 *
 * @param[out] params Stored parameters.
 *
 * @retval 0 Values stored.
 * @retval -RZI_ERR_INVALID params is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend or build lacks CSMA.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_csma_params(struct rzi_lorawan_csma_params *params);

/**
 * @brief Set the CSMA parameters.
 *
 * @param params Parameters to apply.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_INVALID params is NULL or out of range.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend or build lacks CSMA.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_csma_params(const struct rzi_lorawan_csma_params *params);

/**
 * @brief Read the number of uplinks sent since the last downlink.
 *
 * @param[out] count Stored counter.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID count is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not track this counter.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_lost_connection_count(uint16_t *count);

/**
 * @brief Read the number of unanswered uplinks that raises
 *        RZI_LORAWAN_EVENT_NO_DOWNLINK with RZI_LORAWAN_NO_DOWNLINK_USER_THRESHOLD.
 *
 * @param[out] uplinks Stored threshold; zero means disabled.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID uplinks is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_no_downlink_threshold(uint16_t *uplinks);

/**
 * @brief Set the number of unanswered uplinks that raises
 *        RZI_LORAWAN_EVENT_NO_DOWNLINK with RZI_LORAWAN_NO_DOWNLINK_USER_THRESHOLD.
 *
 * @param uplinks Threshold; zero disables the event.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_no_downlink_threshold(uint16_t uplinks);

/**
 * @brief Read the crystal error compensated in receive windows.
 *
 * @param[out] ppm Stored error in parts per million.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID ppm is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this query.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_get_crystal_error(uint32_t *ppm);

/**
 * @brief Set the crystal error compensated in receive windows.
 *
 * @param ppm Error in parts per million.
 *
 * @retval 0 Setting applied.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support this setting.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_set_crystal_error(uint32_t ppm);

/**
 * @brief Request an uplink without application payload.
 *
 * Use it to flush MAC answers or open receive windows. Completion is reported
 * as RZI_LORAWAN_EVENT_TX_DONE, like rzi_lorawan_send().
 *
 * @param with_port Include FPort; false sends a frame with no FPort.
 * @param port FPort, 1 through 223, used when with_port is true.
 * @param type Unconfirmed or confirmed uplink.
 *
 * @retval 0 Request accepted.
 * @retval -RZI_ERR_INVALID port or type is invalid.
 * @retval -RZI_ERR_NOT_JOINED No network session is active.
 * @retval -RZI_ERR_BUSY Another uplink is outstanding.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support empty uplinks.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.4
 */
__must_check int rzi_lorawan_send_empty(bool with_port, uint8_t port,
					enum rzi_lorawan_message_type type);

/** @} */

/** @name Channels
 *
 * US915 and AU915 only. Bit n of word n / 16 is uplink channel n: channels
 * 0-63 are the 125 kHz channels and 64-71 the 500 kHz channels. The core
 * stores one channel mask, persists it with the credentials, and hands it to
 * the backend at the next join; sub-band and fixed-channel calls are views of
 * it. A channel mask from the network (LinkADRReq) replaces it until the next
 * join. Other regions, including CN470, and backends without
 * @ref RZI_LORAWAN_CAP_CHANNEL return -RZI_ERR_NOT_SUPPORTED.
 *  @{
 */

/**
 * @brief Read the channel mask used at the next join.
 *
 * @param[out] mask Buffer of at least 5 words; extra words are cleared.
 * @param words Number of 16-bit words in mask.
 *
 * @retval 0 Mask copied; every channel is set when no mask is stored.
 * @retval -RZI_ERR_INVALID mask is NULL or words is less than 5.
 * @retval -RZI_ERR_NOT_SUPPORTED The region or backend does not support channel masks.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only. Works before @ref rzi_lorawan_start.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_channel_mask(uint16_t *mask, size_t words);

/**
 * @brief Store the channel mask used from the next join on.
 *
 * A mask with every channel set removes the restriction.
 *
 * @param mask Channel-mask words copied before return.
 * @param words Number of 16-bit words in mask, 5 or 6.
 *
 * @retval 0 Mask stored.
 * @retval -RZI_ERR_INVALID mask is NULL, words is out of range, no channel
 *         is set, or a channel above 71 is set.
 * @retval -RZI_ERR_BUSY A join sequence is running.
 * @retval -RZI_ERR_NOT_SUPPORTED The region or backend does not support channel masks.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only. Works before @ref rzi_lorawan_start.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_channel_mask(const uint16_t *mask, size_t words);

/**
 * @brief Read the LoRaWAN frequency sub-band (FSB) of the stored mask.
 *
 * @param[out] sub_band 0 when every channel is enabled, or 1 through 8.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID sub_band is NULL.
 * @retval -RZI_ERR_NO_DATA The mask is not exactly one sub-band.
 * @retval -RZI_ERR_NOT_SUPPORTED The region or backend does not support channel masks.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_sub_band(uint8_t *sub_band);

/**
 * @brief Select a LoRaWAN frequency sub-band (FSB).
 *
 * Sub-band n enables 125 kHz channels 8(n-1) to 8(n-1)+7 and 500 kHz
 * channel 63+n. RUI3 AT+CHE uses blocks without the 500 kHz channel; the AT
 * layer builds those masks with @ref rzi_lorawan_set_channel_mask.
 *
 * @param sub_band 0 for all channels, or 1 through 8.
 *
 * @retval 0 Setting stored.
 * @retval -RZI_ERR_INVALID sub_band is greater than 8.
 * @retval -RZI_ERR_BUSY A join sequence is running.
 * @retval -RZI_ERR_NOT_SUPPORTED The region or backend does not support channel masks.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_sub_band(uint8_t sub_band);

/**
 * @brief Read the fixed uplink channel in hertz.
 *
 * @param[out] frequency_hz Frequency of the only enabled channel, or zero
 *             when more than one channel is enabled.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID frequency_hz is NULL.
 * @retval -RZI_ERR_NOT_SUPPORTED The region or backend does not support channel masks.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_get_fixed_channel(uint32_t *frequency_hz);

/**
 * @brief Restrict uplinks to one channel.
 *
 * US915: 902.3 MHz + 200 kHz * n (n = 0-63) and 903.0 MHz + 1.6 MHz * (n - 64).
 * AU915: 915.2 MHz + 200 kHz * n and 915.9 MHz + 1.6 MHz * (n - 64).
 *
 * @param frequency_hz Channel frequency, or zero to enable every channel.
 *
 * @retval 0 Setting stored.
 * @retval -RZI_ERR_INVALID The frequency is not a regional uplink channel.
 * @retval -RZI_ERR_BUSY A join sequence is running.
 * @retval -RZI_ERR_NOT_SUPPORTED The region or backend does not support channel masks.
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_set_fixed_channel(uint32_t frequency_hz);

/** @} */

/** @name Status
 *  @{
 */

/**
 * @brief Read RSSI of the last application downlink.
 *
 * @param[out] rssi_dbm Stored RSSI in dBm.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID rssi_dbm is NULL.
 * @retval -RZI_ERR_NO_DATA No downlink has been received.
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_is_busy(bool *busy);

/** @} */

/** @name Class B
 *  @{
 */

/**
 * @brief Read Class B ping-slot periodicity.
 *
 * @param[out] periodicity Periodicity in the range 0 through 7.
 *
 * @retval 0 Value stored.
 * @retval -RZI_ERR_INVALID periodicity is NULL.
 * @retval -RZI_ERR_NOT_READY The service has not started.
 * @retval -RZI_ERR_NOT_SUPPORTED The backend does not support Class B.
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_ISR Called from an ISR.
 *
 * @note Thread context only.
 * @since 0.3
 */
__must_check int rzi_lorawan_stop_class_b(void);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif
#endif
