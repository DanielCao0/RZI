/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN key and identifier AT commands.
 */

#include <string.h>

#include <zephyr/sys/byteorder.h>
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

static int handle_devaddr(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	uint8_t bytes[4];
	char hex[9];
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		sys_put_be32(context->dev_addr, bytes);
		rzi_at_lorawan_bin_to_hex(bytes, sizeof(bytes), hex);
		return rzi_at_respond_value("AT+DEVADDR=%s", hex);
	}
	rc = rzi_at_lorawan_hex_to_bin(request->argument, bytes, sizeof(bytes));
	if (rc != 0) {
		return rc;
	}
	context->dev_addr = sys_get_be32(bytes);
	rc = rzi_at_lorawan_nvm_save("devaddr", &context->dev_addr, sizeof(context->dev_addr));
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_netid(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	uint8_t bytes[3];
	char hex[7];
	uint32_t net_id = 0;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		if (rzi_at_lorawan_is_joined() && rzi_lorawan_get_net_id(&net_id) == 0) {
			context->net_id = net_id;
			context->net_id_valid = true;
		} else if (!context->net_id_valid) {
			return -RZI_ERR_NO_DATA;
		}
		bytes[0] = (uint8_t)((context->net_id >> 16) & 0xffU);
		bytes[1] = (uint8_t)((context->net_id >> 8) & 0xffU);
		bytes[2] = (uint8_t)(context->net_id & 0xffU);
		rzi_at_lorawan_bin_to_hex(bytes, sizeof(bytes), hex);
		return rzi_at_respond_value("AT+NETID=%s", hex);
	}
	rc = rzi_at_lorawan_hex_to_bin(request->argument, bytes, sizeof(bytes));
	if (rc != 0) {
		return rc;
	}
	context->net_id = ((uint32_t)bytes[0] << 16) | ((uint32_t)bytes[1] << 8) | bytes[2];
	context->net_id_valid = true;
	rc = rzi_at_lorawan_nvm_save("netid", &context->net_id, sizeof(context->net_id));
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_mcrootkey(const struct rzi_at_request *request, void *user_data)
{
	char hex[33];

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	rzi_at_lorawan_bin_to_hex(rzi_at_lorawan_context.app_key,
				  sizeof(rzi_at_lorawan_context.app_key), hex);
	return rzi_at_respond_value("AT+MCROOTKEY=%s", hex);
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

static struct key_command nwk_skey = {
	.response_name = "AT+NWKSKEY",
	.nvm_key = "nwkskey",
	.value = rzi_at_lorawan_context.nwk_skey,
	.size = sizeof(rzi_at_lorawan_context.nwk_skey),
};

static struct key_command app_skey = {
	.response_name = "AT+APPSKEY",
	.nvm_key = "appskey",
	.value = rzi_at_lorawan_context.app_skey,
	.size = sizeof(rzi_at_lorawan_context.app_skey),
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
	{
		.name = "DEVADDR",
		.help = "get or set the device address (4 bytes in hex)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_devaddr,
	},
	{
		.name = "NWKSKEY",
		.help = "get or set the network session key (16 bytes in hex)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_key,
		.user_data = &nwk_skey,
	},
	{
		.name = "APPSKEY",
		.help = "get or set the application session key (16 bytes in hex)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_key,
		.user_data = &app_skey,
	},
	{
		.name = "NETID",
		.help = "get or set the network identifier (NetID) (3 bytes in hex)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_netid,
	},
	{
		.name = "MCROOTKEY",
		.help = "get the multicast root key (16 bytes in hex)",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_mcrootkey,
	},
};

const struct rzi_at_lorawan_command_group rzi_at_lorawan_key_id_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
