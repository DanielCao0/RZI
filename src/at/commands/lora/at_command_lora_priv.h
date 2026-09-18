/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Internal contract shared by raw LoRa AT command groups.
 */
#ifndef RZI_AT_COMMAND_LORA_PRIV_H
#define RZI_AT_COMMAND_LORA_PRIV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <rzi/at/at.h>
#include <rzi/lora/lora.h>

/** One cohesive group of raw LoRa AT command descriptors. */
struct rzi_at_lora_command_group {
	/** Static-lifetime command descriptor array. */
	const struct rzi_at_command *commands;
	/** Number of descriptors in commands. */
	size_t count;
};

/** P2P parameter and send/receive command descriptors. */
extern const struct rzi_at_lora_command_group rzi_at_lora_p2p_group;
/** Radio certification test command descriptors. */
extern const struct rzi_at_lora_command_group rzi_at_lora_test_group;

/** Convert an exact-length hexadecimal string into bytes. */
int rzi_at_lora_hex_to_bin(const char *hex, uint8_t *out, size_t out_len);

/** Convert bytes into an uppercase, NUL-terminated hexadecimal string. */
void rzi_at_lora_bin_to_hex(const uint8_t *in, size_t len, char *out);

/** Parse a base-10 integer command argument. */
int rzi_at_lora_parse_long(const char *argument, long *value);

/** Parse a 0/1 boolean command argument. */
int rzi_at_lora_parse_bool(const char *argument, bool *value);

/** Ensure the raw LoRa service is started for the current network mode. */
int rzi_at_lora_ensure_started(void);

/** Apply a network-mode change from AT+NWM without requiring a reboot. */
void rzi_at_lora_on_network_mode(uint8_t mode);

/** Return the current working mode: 0 = P2P LoRa, 1 = LoRaWAN, 2 = P2P FSK. */
uint8_t rzi_at_lora_network_mode(void);

#endif /* RZI_AT_COMMAND_LORA_PRIV_H */
