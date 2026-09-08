/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN activation, uplink, and downlink AT commands.
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include "at_command_lorawan_priv.h"

struct join_parameters {
	bool start;
	bool auto_join;
	uint8_t interval;
	uint8_t attempts;
};

static int handle_njm(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);

	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+NJM=1");
	}
	/* OTAA is implemented; ABP remains a planned service. */
	return strcmp(request->argument, "1") == 0 ? rzi_at_respond_status(RZI_AT_STATUS_OK)
						   : -EINVAL;
}

static int handle_njs(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_value("AT+NJS=%d", rzi_at_lorawan_is_joined() ? 1 : 0);
}

static int handle_cfm(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	bool enabled;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+CFM=%d",
					    atomic_get(&context->confirmed_uplink) ? 1 : 0);
	}
	if (strcmp(request->argument, "0") == 0) {
		enabled = false;
		atomic_clear(&context->confirmed_uplink);
	} else if (strcmp(request->argument, "1") == 0) {
		enabled = true;
		atomic_set(&context->confirmed_uplink, 1);
	} else {
		return -EINVAL;
	}
	int rc = rzi_at_lorawan_nvm_save("cfm", &enabled, sizeof(enabled));

	if (rc != 0) {
		return rc;
	}
	return rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_cfs(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_value(
		"AT+CFS=%d", atomic_get(&rzi_at_lorawan_context.confirmation_status) ? 1 : 0);
}

static int parse_join_parameters(const char *argument, struct join_parameters *parameters)
{
	const struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	long values[4];
	size_t count = 0;
	const char *cursor = argument;

	while (*cursor != '\0' && count < ARRAY_SIZE(values)) {
		char *end;

		values[count] = strtol(cursor, &end, 10);
		if (end == cursor || values[count] < 0 || values[count] > UINT8_MAX) {
			return -EINVAL;
		}
		++count;
		if (*end == '\0') {
			cursor = end;
			break;
		}
		if (*end != ':' || end[1] == '\0') {
			return -EINVAL;
		}
		cursor = end + 1;
	}

	if (*cursor != '\0' || count == 0 || values[0] > 1 || (count >= 2 && values[1] > 1) ||
	    (count >= 3 && (values[2] < RZI_AT_LORAWAN_JOIN_INTERVAL_MIN ||
			    values[2] > RZI_AT_LORAWAN_JOIN_INTERVAL_MAX))) {
		return -EINVAL;
	}

	parameters->start = values[0] != 0;
	parameters->auto_join = count >= 2 ? values[1] != 0 : context->auto_join;
	parameters->interval = count >= 3 ? (uint8_t)values[2] : context->join_interval;
	parameters->attempts = count >= 4 ? (uint8_t)values[3] : context->join_attempts;
	return 0;
}

static int save_join_parameters(const struct join_parameters *parameters)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	int rc;

	context->auto_join = parameters->auto_join;
	context->join_interval = parameters->interval;
	context->join_attempts = parameters->attempts;
	rc = rzi_at_lorawan_nvm_save("autojoin", &context->auto_join, sizeof(context->auto_join));
	if (rc == 0) {
		rc = rzi_at_lorawan_nvm_save("join_interval", &context->join_interval,
					     sizeof(context->join_interval));
	}
	if (rc == 0) {
		rc = rzi_at_lorawan_nvm_save("join_attempts", &context->join_attempts,
					     sizeof(context->join_attempts));
	}
	return rc;
}

static int respond_to_result(int rc)
{
	return rc == 0 ? rzi_at_respond_status(RZI_AT_STATUS_OK) : rc;
}

static int handle_join(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+JOIN=1:%u:%u:%u", context->auto_join ? 1U : 0U,
					    context->join_interval, context->join_attempts);
	}
	if (request->operation == RZI_AT_OP_RUN) {
		return respond_to_result(rzi_at_lorawan_join_start());
	}

	struct join_parameters parameters;
	int rc = parse_join_parameters(request->argument, &parameters);

	if (rc != 0) {
		return rc;
	}
	if (parameters.start && atomic_get(&context->join_sequence_active)) {
		return -EBUSY;
	}
	rc = save_join_parameters(&parameters);
	if (rc != 0) {
		return rc;
	}
	return respond_to_result(parameters.start ? rzi_at_lorawan_join_start()
						  : rzi_at_lorawan_join_stop());
}

static int handle_send(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	uint8_t payload[RZI_LORAWAN_MAX_PAYLOAD];
	const char *colon = strchr(request->argument, ':');
	char *port_end;
	const char *hex;
	long port;
	size_t hex_len;
	bool confirmed;
	int rc;

	ARG_UNUSED(user_data);
	if (colon == NULL) {
		return -EINVAL;
	}
	port = strtol(request->argument, &port_end, 10);
	if (port_end != colon || port < 1 || port > 223) {
		return -EINVAL;
	}

	hex = colon + 1;
	hex_len = strlen(hex);
	if (hex_len == 0 || hex_len > sizeof(payload) * 2U) {
		return -EINVAL;
	}
	rc = rzi_at_lorawan_hex_to_bin(hex, payload, hex_len / 2U);
	if (rc != 0) {
		return rc;
	}
	if (!rzi_at_lorawan_is_joined()) {
		return -ENETDOWN;
	}

	confirmed = atomic_get(&context->confirmed_uplink) != 0;
	atomic_set(&context->tx_confirmed, confirmed);
	rc = rzi_lorawan_send((uint8_t)port, payload, hex_len / 2U,
			      confirmed ? RZI_LORAWAN_MSG_CONFIRMED : RZI_LORAWAN_MSG_UNCONFIRMED);
	return respond_to_result(rc);
}

static int handle_recv(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	uint8_t data[RZI_LORAWAN_MAX_PAYLOAD];
	char hex[RZI_LORAWAN_MAX_PAYLOAD * 2U + 1U];
	uint8_t port = 0;
	uint8_t size = 0;
	bool valid;

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	k_mutex_lock(&context->state_lock, K_FOREVER);
	valid = context->last_downlink.valid;
	if (valid) {
		context->last_downlink.valid = false;
		port = context->last_downlink.port;
		size = context->last_downlink.size;
		memcpy(data, context->last_downlink.data, size);
	}
	k_mutex_unlock(&context->state_lock);

	if (!valid) {
		return rzi_at_respond_value("AT+RECV=");
	}
	rzi_at_lorawan_bin_to_hex(data, size, hex);
	return rzi_at_respond_value("AT+RECV=%u:%s", port, hex);
}

static const struct rzi_at_command commands[] = {
	{
		.name = "NJM",
		.help = "get or set the network join mode (0 = ABP, 1 = OTAA)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_njm,
	},
	{
		.name = "NJS",
		.help = "get the join status (0 = not joined, 1 = joined)",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_njs,
	},
	{
		.name = "CFM",
		.help = "get or set the confirmation mode (0 = OFF, 1 = ON)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_cfm,
	},
	{
		.name = "CFS",
		.help = "get the confirmation status of the last AT+SEND (0 = failure, 1 = "
			"success)",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_cfs,
	},
	{
		.name = "JOIN",
		.help = "join network",
		.allowed_operations = RZI_AT_ALLOW_RUN | RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_join,
	},
	{
		.name = "SEND",
		.help = "send data along with the application port",
		.allowed_operations = RZI_AT_ALLOW_WRITE,
		.handler = handle_send,
	},
	{
		.name = "RECV",
		.help = "print the last received data in hex format",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_recv,
	},
};

const struct rzi_at_lorawan_command_group rzi_at_lorawan_join_send_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
