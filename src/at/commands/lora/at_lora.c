/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Raw LoRa AT package lifecycle and command registration.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "../../at_priv.h"
#include "../at_network_mode.h"
#include "at_lora_priv.h"

static __maybe_unused void ignore_result(int result)
{
	ARG_UNUSED(result);
}

static int hex_nibble(char value)
{
	if (value >= '0' && value <= '9') {
		return value - '0';
	}
	if (value >= 'A' && value <= 'F') {
		return value - 'A' + 10;
	}
	if (value >= 'a' && value <= 'f') {
		return value - 'a' + 10;
	}
	return -1;
}

int rzi_at_lora_hex_to_bin(const char *hex, uint8_t *out, size_t out_len)
{
	size_t len = strlen(hex);

	if (len != out_len * 2U) {
		return -RZI_ERR_INVALID;
	}
	for (size_t i = 0; i < out_len; ++i) {
		int high = hex_nibble(hex[i * 2U]);
		int low = hex_nibble(hex[i * 2U + 1U]);

		if (high < 0 || low < 0) {
			return -RZI_ERR_INVALID;
		}
		out[i] = (uint8_t)((high << 4) | low);
	}
	return 0;
}

void rzi_at_lora_bin_to_hex(const uint8_t *in, size_t len, char *out)
{
	static const char digits[] = "0123456789ABCDEF";

	for (size_t i = 0; i < len; ++i) {
		out[i * 2U] = digits[in[i] >> 4];
		out[i * 2U + 1U] = digits[in[i] & 0x0f];
	}
	out[len * 2U] = '\0';
}

int rzi_at_lora_parse_long(const char *argument, long *value)
{
	char *end = NULL;

	if (argument == NULL || value == NULL) {
		return -RZI_ERR_INVALID;
	}
	*value = strtol(argument, &end, 10);
	if (end == argument || *end != '\0') {
		return -RZI_ERR_INVALID;
	}
	return 0;
}

int rzi_at_lora_parse_bool(const char *argument, bool *value)
{
	if (strcmp(argument, "0") == 0) {
		*value = false;
		return 0;
	}
	if (strcmp(argument, "1") == 0) {
		*value = true;
		return 0;
	}
	return -RZI_ERR_INVALID;
}

static void on_network_mode(uint8_t mode)
{
	if (mode == RZI_AT_NETWORK_MODE_LORAWAN) {
		ignore_result(rzi_lora_stop());
		return;
	}
	ignore_result(rzi_lora_start(mode == RZI_AT_NETWORK_MODE_P2P_FSK ? RZI_LORA_MOD_FSK
									 : RZI_LORA_MOD_LORA));
}

int rzi_at_lora_ensure_started(void)
{
	uint8_t mode = rzi_at_network_mode_get();

	if (mode == RZI_AT_NETWORK_MODE_LORAWAN) {
		return -RZI_ERR_BUSY;
	}
	if (rzi_lora_is_started()) {
		return 0;
	}
	return rzi_lora_start(mode == RZI_AT_NETWORK_MODE_P2P_FSK ? RZI_LORA_MOD_FSK
								  : RZI_LORA_MOD_LORA);
}

static void on_tx_done(int error, void *user_data)
{
	ARG_UNUSED(user_data);
	if (error == 0) {
		ignore_result(rzi_at_publish_event("TXP2P DONE"));
	} else {
		ignore_result(rzi_at_publish_event("TXP2P ERROR"));
	}
}

static void on_rx_done(const uint8_t *data, size_t size, int16_t rssi_dbm, int8_t snr_quarter_db,
		       void *user_data)
{
	char hex[RZI_LORA_MAX_PAYLOAD * 2U + 1U];

	ARG_UNUSED(user_data);
	rzi_at_lora_bin_to_hex(data, size, hex);
	ignore_result(rzi_at_publish_event("RXP2P:%d:%d:%s", rssi_dbm, snr_quarter_db / 4, hex));
}

static const struct rzi_lora_callbacks callbacks = {
	.tx_done = on_tx_done,
	.rx_done = on_rx_done,
};

static int extension_start(void)
{
	int rc = rzi_lora_register_callbacks(&callbacks);

	if (rc != 0) {
		return rc;
	}
	if (rzi_at_network_mode_get() != RZI_AT_NETWORK_MODE_LORAWAN) {
		rc = rzi_at_lora_ensure_started();
	}
	return rc;
}

static void extension_process(void)
{
	rzi_lora_poll();
}

static const struct rzi_at_extension extension = {
	.start = extension_start,
	.process = extension_process,
};

int rzi_at_lora_register(void)
{
	static const struct rzi_at_lora_command_group *const groups[] = {
		&rzi_at_lora_p2p_group,
		&rzi_at_lora_test_group,
	};

	int rc = rzi_at_network_mode_add_listener(on_network_mode);

	if (rc != 0) {
		return rc;
	}
	for (size_t i = 0; i < ARRAY_SIZE(groups); ++i) {
		rc = rzi_at_register(groups[i]->commands, groups[i]->count);
		if (rc != 0) {
			return rc;
		}
	}
	return rzi_at_registry_add_extension(&extension);
}
