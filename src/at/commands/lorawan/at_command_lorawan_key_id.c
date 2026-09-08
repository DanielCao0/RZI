/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN key and identifier AT commands.
 */

#include <string.h>

#include <zephyr/sys/util.h>

#include "at_command_lorawan_priv.h"

struct key_command {
	const char *response_name;
	const char *nvm_key;
	uint8_t *value;
	size_t size;
};

static int handle_key(const struct rzi_at_request *request, void *user_data)
{
	const struct key_command *command = user_data;

	if (request->operation == RZI_AT_OP_READ) {
		char hex[33];

		rzi_at_lorawan_bin_to_hex(command->value, command->size, hex);
		return rzi_at_respond_value("%s=%s", command->response_name, hex);
	}

	uint8_t parsed[16];
	int rc = rzi_at_lorawan_hex_to_bin(request->argument, parsed, command->size);

	if (rc != 0) {
		return rc;
	}
	memcpy(command->value, parsed, command->size);
	rc = rzi_at_lorawan_nvm_save(command->nvm_key, command->value, command->size);
	if (rc != 0) {
		return rc;
	}
	return rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static struct key_command dev_eui = {
	.response_name = "AT+DEVEUI",
	.nvm_key = "deveui",
	.value = rzi_at_lorawan_context.dev_eui,
	.size = sizeof(rzi_at_lorawan_context.dev_eui),
};

static struct key_command join_eui = {
	.response_name = "AT+APPEUI",
	.nvm_key = "joineui",
	.value = rzi_at_lorawan_context.join_eui,
	.size = sizeof(rzi_at_lorawan_context.join_eui),
};

static struct key_command app_key = {
	.response_name = "AT+APPKEY",
	.nvm_key = "appkey",
	.value = rzi_at_lorawan_context.app_key,
	.size = sizeof(rzi_at_lorawan_context.app_key),
};

static const struct rzi_at_command commands[] = {
	{
		.name = "DEVEUI",
		.help = "get or set the device EUI (8 bytes in hex)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_key,
		.user_data = &dev_eui,
	},
	{
		.name = "APPEUI",
		.help = "get or set the application EUI (8 bytes in hex)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_key,
		.user_data = &join_eui,
	},
	{
		.name = "APPKEY",
		.help = "get or set the application key (16 bytes in hex)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_key,
		.user_data = &app_key,
	},
};

const struct rzi_at_lorawan_command_group rzi_at_lorawan_key_id_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
