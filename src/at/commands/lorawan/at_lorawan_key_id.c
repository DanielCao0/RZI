/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN key and identifier AT commands.
 */

#include <string.h>

#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include "at_lorawan_priv.h"

struct key_command {
	const char *response_name;
	size_t size;
	int (*get)(uint8_t *value, size_t len);
	int (*set)(const uint8_t *value, size_t len);
};

static int handle_key(const struct rzi_at_request *request, void *user_data)
{
	const struct key_command *command = user_data;
	uint8_t value[16];
	int rc;

	if (request->operation == RZI_AT_OP_READ) {
		char hex[33];

		rc = command->get(value, command->size);
		if (rc == -RZI_ERR_NO_DATA) {
			memset(value, 0, command->size);
		} else if (rc != 0) {
			return rc;
		}
		rzi_at_lorawan_bin_to_hex(value, command->size, hex);
		return rzi_at_respond_value("%s=%s", command->response_name, hex);
	}

	rc = rzi_at_lorawan_hex_to_bin(request->argument, value, command->size);
	if (rc != 0) {
		return rc;
	}
	rc = command->set(value, command->size);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_devaddr(const struct rzi_at_request *request, void *user_data)
{
	uint8_t bytes[4];
	char hex[9];
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		uint32_t dev_addr = 0;

		rc = rzi_lorawan_get_dev_addr(&dev_addr);
		if (rc == -RZI_ERR_NO_DATA) {
			dev_addr = 0;
		} else if (rc != 0) {
			return rc;
		}
		sys_put_be32(dev_addr, bytes);
		rzi_at_lorawan_bin_to_hex(bytes, sizeof(bytes), hex);
		return rzi_at_respond_value("AT+DEVADDR=%s", hex);
	}
	rc = rzi_at_lorawan_hex_to_bin(request->argument, bytes, sizeof(bytes));
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_set_dev_addr(sys_get_be32(bytes));
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
	uint8_t key[16] = {0};
	char hex[33];
	int rc;

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	rc = rzi_lorawan_get_app_key(key, sizeof(key));
	if (rc != 0 && rc != -RZI_ERR_NO_DATA) {
		return rc;
	}
	rzi_at_lorawan_bin_to_hex(key, sizeof(key), hex);
	return rzi_at_respond_value("AT+MCROOTKEY=%s", hex);
}

static struct key_command dev_eui = {
	.response_name = "AT+DEVEUI",
	.size = 8,
	.get = rzi_lorawan_get_dev_eui,
	.set = rzi_lorawan_set_dev_eui,
};

static struct key_command join_eui = {
	.response_name = "AT+APPEUI",
	.size = 8,
	.get = rzi_lorawan_get_app_eui,
	.set = rzi_lorawan_set_app_eui,
};

static struct key_command app_key = {
	.response_name = "AT+APPKEY",
	.size = 16,
	.get = rzi_lorawan_get_app_key,
	.set = rzi_lorawan_set_app_key,
};

static struct key_command nwk_skey = {
	.response_name = "AT+NWKSKEY",
	.size = 16,
	.get = rzi_lorawan_get_nwk_skey,
	.set = rzi_lorawan_set_nwk_skey,
};

static struct key_command app_skey = {
	.response_name = "AT+APPSKEY",
	.size = 16,
	.get = rzi_lorawan_get_app_skey,
	.set = rzi_lorawan_set_app_skey,
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
