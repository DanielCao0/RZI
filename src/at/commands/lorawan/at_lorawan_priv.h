/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Internal contract shared by LoRaWAN AT command groups.
 */
#ifndef RZI_AT_COMMAND_LORAWAN_PRIV_H
#define RZI_AT_COMMAND_LORAWAN_PRIV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <rzi/at/at.h>
#include <rzi/lorawan/lorawan.h>

/** Default delay between join attempts in seconds. */
#define RZI_AT_LORAWAN_JOIN_INTERVAL_DEFAULT 8U
/** Minimum RUI3-compatible join retry interval. */
#define RZI_AT_LORAWAN_JOIN_INTERVAL_MIN     7U
/** Maximum RUI3-compatible join retry interval. */
#define RZI_AT_LORAWAN_JOIN_INTERVAL_MAX     255U

/** Last application downlink retained for AT+RECV. */
struct rzi_at_lorawan_last_downlink {
	/** True while one unread downlink is stored. */
	bool valid;
	/** LoRaWAN application port. */
	uint8_t port;
	/** Number of valid bytes in data. */
	uint8_t size;
	/** Value-owned payload storage. */
	uint8_t data[RZI_LORAWAN_MAX_PAYLOAD];
};

/** Process-wide state owned by the LoRaWAN AT command package. */
struct rzi_at_lorawan_context {
	/** True after the public LoRaWAN service has started. */
	bool service_started;
	/** Defers the first join until backend readiness. */
	atomic_t join_pending;
	/** Guards one active manual or automatic join sequence. */
	atomic_t join_sequence_active;
	/** Number of retries remaining after the current attempt. */
	atomic_t join_retries_remaining;
	/** Registration handle for service callbacks. */
	rzi_lorawan_callback_handle_t callback_handle;
	/** OTAA device EUI. */
	uint8_t dev_eui[8];
	/** OTAA join EUI, exposed as APPEUI for RUI3 compatibility. */
	uint8_t join_eui[8];
	/** OTAA application key. */
	uint8_t app_key[16];
	/** ABP device address, host byte order. */
	uint32_t dev_addr;
	/** ABP network session key. */
	uint8_t nwk_skey[16];
	/** ABP application session key. */
	uint8_t app_skey[16];
	/** Stored NetID, 24-bit value. */
	uint32_t net_id;
	/** True when AT+NETID has been written. */
	bool net_id_valid;
	/** Network join mode: 1 = OTAA, 0 = ABP. */
	uint8_t join_mode;
	/** Confirmed-uplink retransmission count for AT+RETY. */
	uint8_t retries;
	/** Region selected by AT+BAND. */
	enum rzi_lorawan_region region;
	/** Device class selected by AT+CLASS. */
	enum rzi_lorawan_class device_class;
	/** Join configuration retained until backend readiness. */
	struct rzi_lorawan_join_config pending_join_config;
	/** Delayed work used for join retries. */
	struct k_work_delayable join_retry_work;
	/** Whether a join sequence starts automatically after boot. */
	bool auto_join;
	/** Delay between join attempts in seconds. */
	uint8_t join_interval;
	/** Number of retries after the initial join attempt. */
	uint8_t join_attempts;
	/** Confirmed-uplink setting selected by AT+CFM. */
	atomic_t confirmed_uplink;
	/** Last confirmed-uplink result exposed by AT+CFS. */
	atomic_t confirmation_status;
	/** Confirmation mode captured for the outstanding uplink. */
	atomic_t tx_confirmed;
	/** Protects retained downlink state. */
	struct k_mutex state_lock;
	/** Most recent unread application downlink. */
	struct rzi_at_lorawan_last_downlink last_downlink;
};

/** One cohesive group of LoRaWAN AT command descriptors. */
struct rzi_at_lorawan_command_group {
	/** Static-lifetime command descriptor array. */
	const struct rzi_at_command *commands;
	/** Number of descriptors in commands. */
	size_t count;
};

/** Shared package state. */
extern struct rzi_at_lorawan_context rzi_at_lorawan_context;
/** Key and identifier command descriptors. */
extern const struct rzi_at_lorawan_command_group rzi_at_lorawan_key_id_group;
/** Join and application-data command descriptors. */
extern const struct rzi_at_lorawan_command_group rzi_at_lorawan_join_send_group;
/** Network management command descriptors. */
extern const struct rzi_at_lorawan_command_group rzi_at_lorawan_network_management_group;
/** Supplementary channel-plan command descriptors. */
extern const struct rzi_at_lorawan_command_group rzi_at_lorawan_supplementary_group;
/** Runtime information command descriptors. */
extern const struct rzi_at_lorawan_command_group rzi_at_lorawan_information_group;
/** Class B command descriptors. */
extern const struct rzi_at_lorawan_command_group rzi_at_lorawan_class_b_group;
/** Multicast command descriptors. */
extern const struct rzi_at_lorawan_command_group rzi_at_lorawan_multicast_group;
/** Certification command descriptors. */
extern const struct rzi_at_lorawan_command_group rzi_at_lorawan_certification_group;

/** Convert an exact-length hexadecimal string into bytes. */
int rzi_at_lorawan_hex_to_bin(const char *hex, uint8_t *out, size_t out_len);

/** Convert bytes into an uppercase, NUL-terminated hexadecimal string. */
void rzi_at_lorawan_bin_to_hex(const uint8_t *in, size_t len, char *out);

/** Parse a base-10 integer command argument. */
int rzi_at_lorawan_parse_long(const char *argument, long *value);

/** Parse a 0/1 boolean command argument. */
int rzi_at_lorawan_parse_bool(const char *argument, bool *value);

/** Apply AT+CLASS after the LoRaWAN service has started. */
int rzi_at_lorawan_apply_class(void);

/** Map one RUI3 AT+BAND number to an RZI region. */
int rzi_at_lorawan_band_to_region(int band, enum rzi_lorawan_region *region);

/** Map one RZI region to its RUI3 AT+BAND number. */
int rzi_at_lorawan_region_to_band(enum rzi_lorawan_region region);

/** Persist one LoRaWAN AT setting when NVM support is enabled. */
__must_check int rzi_at_lorawan_nvm_save(const char *key, const void *value, size_t len);

/** Return whether the LoRaWAN service has an active session. */
bool rzi_at_lorawan_is_joined(void);

/** Start one configured join sequence. */
int rzi_at_lorawan_join_start(void);

/** Stop joining, cancel retries, and leave the current session. */
int rzi_at_lorawan_join_stop(void);

#endif /* RZI_AT_COMMAND_LORAWAN_PRIV_H */
