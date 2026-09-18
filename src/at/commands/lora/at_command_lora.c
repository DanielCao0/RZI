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
#include "at_command_lora_priv.h"

#if defined(CONFIG_RZI_AT_COMMAND_LORAWAN)
#include "../lorawan/at_command_lorawan_priv.h"
#endif

#if !defined(CONFIG_RZI_AT_COMMAND_LORAWAN)
static uint8_t network_mode;
#endif

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
		return -EINVAL;
	}
	for (size_t i = 0; i < out_len; ++i) {
		int high = hex_nibble(hex[i * 2U]);
		int low = hex_nibble(hex[i * 2U + 1U]);

		if (high < 0 || low < 0) {
			return -EINVAL;
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
		return -EINVAL;
	}
	*value = strtol(argument, &end, 10);
	if (end == argument || *end != '\0') {
		return -EINVAL;
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
	return -EINVAL;
}

uint8_t rzi_at_lora_network_mode(void)
{
#if defined(CONFIG_RZI_AT_COMMAND_LORAWAN)
	return rzi_at_lorawan_context.network_mode;
#else
	return network_mode;
#endif
}

void rzi_at_lora_on_network_mode(uint8_t mode)
{
#if !defined(CONFIG_RZI_AT_COMMAND_LORAWAN)
	network_mode = mode;
#endif
	if (mode == 1U) {
		(void)rzi_lora_stop();
		return;
	}
	(void)rzi_lora_start(mode == 2U ? RZI_LORA_MOD_FSK : RZI_LORA_MOD_LORA);
}

int rzi_at_lora_ensure_started(void)
{
	uint8_t mode = rzi_at_lora_network_mode();

	if (mode == 1U) {
		return -EBUSY;
	}
	if (rzi_lora_is_started()) {
		return 0;
	}
	return rzi_lora_start(mode == 2U ? RZI_LORA_MOD_FSK : RZI_LORA_MOD_LORA);
}

static void on_tx_done(int error, void *user_data)
{
	ARG_UNUSED(user_data);
	if (error == 0) {
		(void)rzi_at_publish_event("TXP2P DONE");
	} else {
		(void)rzi_at_publish_event("TXP2P ERROR");
	}
}

static void on_rx_done(const uint8_t *data, size_t size, int16_t rssi_dbm, int8_t snr_quarter_db,
		       void *user_data)
{
	char hex[RZI_LORA_MAX_PAYLOAD * 2U + 1U];

	ARG_UNUSED(user_data);
	rzi_at_lora_bin_to_hex(data, size, hex);
	(void)rzi_at_publish_event("RXP2P:%d:%d:%s", rssi_dbm, snr_quarter_db / 4, hex);
}

static const struct rzi_lora_callbacks callbacks = {
	.tx_done = on_tx_done,
	.rx_done = on_rx_done,
};

#if !defined(CONFIG_RZI_AT_COMMAND_LORAWAN)
static int handle_nwm(const struct rzi_at_request *request, void *user_data)
{
	long parsed;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+NWM=%u", network_mode);
	}
	rc = rzi_at_lora_parse_long(request->argument, &parsed);
	if (rc != 0 || parsed < 0 || parsed > 2) {
		return -EINVAL;
	}
	rzi_at_lora_on_network_mode((uint8_t)parsed);
	return rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static const struct rzi_at_command nwm_command = {
	.name = "NWM",
	.help = "get or set the network working mode (0 = P2P_LORA, 1 = LoRaWAN, 2 = P2P_FSK)",
	.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
	.handler = handle_nwm,
};
#endif

static int extension_start(void)
{
	int rc = rzi_lora_register_callbacks(&callbacks);

	if (rc != 0) {
		return rc;
	}
	if (rzi_at_lora_network_mode() != 1U) {
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

#if !defined(CONFIG_RZI_AT_COMMAND_LORAWAN)
	int rc = rzi_at_register(&nwm_command, 1);

	if (rc != 0) {
		return rc;
	}
#else
	int rc = 0;
#endif
	for (size_t i = 0; i < ARRAY_SIZE(groups); ++i) {
		rc = rzi_at_register(groups[i]->commands, groups[i]->count);
		if (rc != 0) {
			return rc;
		}
	}
	return rzi_at_registry_add_extension(&extension);
}
