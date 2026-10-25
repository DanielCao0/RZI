/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Private helpers shared by LoRaWAN public implementations.
 */
#ifndef RZI_LORAWAN_PRIV_H
#define RZI_LORAWAN_PRIV_H

#include <stdbool.h>
#include <stdint.h>

#include <rzi/lorawan/lorawan.h>

/** OTAA credentials copied into the backend before join returns. */
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

/** Network activation parameters passed to one backend. */
struct rzi_lorawan_join_config {
	/** Selects the active union member. */
	enum rzi_lorawan_activation activation;
	/**
	 * Seconds between attempts for backends without their own join backoff.
	 * USP ignores it and applies the LoRaWAN regional join duty cycle.
	 */
	uint8_t retry_interval_s;
	/** True when channel_mask limits the uplink channels. */
	bool channel_mask_set;
	/**
	 * Uplink channels for the join and the post-join defaults, in the
	 * layout of @ref rzi_lorawan_set_channel_mask. A network LinkADRReq
	 * replaces it until the next join.
	 */
	uint16_t channel_mask[RZI_LORAWAN_CHANNEL_MASK_WORDS];
	union {
		/** Parameters used when activation is OTAA. */
		struct rzi_lorawan_join_otaa otaa;
		/** Parameters used when activation is ABP. */
		struct rzi_lorawan_join_abp abp;
	};
};

/** US915 and AU915: 64 125 kHz uplink channels followed by 8 500 kHz channels. */
#define RZI_LORAWAN_FIXED_PLAN_CHANNELS 72U
/** First 500 kHz channel of US915 and AU915. */
#define RZI_LORAWAN_FIXED_PLAN_WIDE     64U
/** Mask words that cover @ref RZI_LORAWAN_FIXED_PLAN_CHANNELS. */
#define RZI_LORAWAN_FIXED_PLAN_WORDS    5U

/** Return true when region has a channel plan the channel-mask API supports. */
bool rzi_lorawan_region_has_fixed_plan(enum rzi_lorawan_region region);

/**
 * Read the stored channel mask; set reports whether one is stored.
 * mask holds @ref RZI_LORAWAN_CHANNEL_MASK_WORDS words.
 */
int rzi_lorawan_credentials_get_channel_mask(uint16_t *mask, bool *set);

/** Store and persist a validated channel mask, or clear it when mask is NULL. */
int rzi_lorawan_credentials_set_channel_mask(const uint16_t *mask);

/* Caller serializes region access with config_lock. */
int rzi_lorawan_config_get_region(enum rzi_lorawan_region *region);
int rzi_lorawan_config_set_region(enum rzi_lorawan_region region);
int rzi_lorawan_config_get_net_id(uint32_t *net_id);

int rzi_lorawan_check_thread(void);
int rzi_lorawan_check_started(void);
void rzi_lorawan_note_class(enum rzi_lorawan_class device_class);
void rzi_lorawan_mac_commands_on_uplink(void);
/** Store join policy fields. @ref RZI_LORAWAN_JOIN_KEEP leaves a field unchanged. */
int rzi_lorawan_credentials_update_policy(int32_t auto_join, int32_t interval, int32_t attempts);

/** Fill one backend join configuration from the stored credentials. */
int rzi_lorawan_credentials_build_config(struct rzi_lorawan_join_config *config);

/** Mark whether a join sequence currently owns the stored credentials. */
void rzi_lorawan_credentials_set_join_active(bool active);

#endif /* RZI_LORAWAN_PRIV_H */
