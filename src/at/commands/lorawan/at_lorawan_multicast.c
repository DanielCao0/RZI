/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN multicast AT commands.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/sys/util.h>

#include <rzi/lorawan/multicast.h>

#include "at_lorawan_priv.h"

static int parse_dev_addr(const char *hex, uint32_t *dev_addr)
{
	uint8_t bytes[4];
	int rc = rzi_at_lorawan_hex_to_bin(hex, bytes, sizeof(bytes));

	if (rc != 0) {
		return rc;
	}
	*dev_addr = ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
		    ((uint32_t)bytes[2] << 8) | bytes[3];
	return 0;
}

static int handle_addmulc(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_lorawan_multicast_session session = {
		.group_id = -1,
	};
	char class_letter;
	char dev_addr[9];
	char nwk_skey[33];
	char app_skey[33];
	unsigned int frequency_hz;
	unsigned int data_rate;
	unsigned int periodicity;
	int rc;

	ARG_UNUSED(user_data);
	if (sscanf(request->argument, "%c:%8[0-9A-Fa-f]:%32[0-9A-Fa-f]:%32[0-9A-Fa-f]:%u:%u:%u",
		   &class_letter, dev_addr, nwk_skey, app_skey, &frequency_hz, &data_rate,
		   &periodicity) != 7) {
		return -RZI_ERR_INVALID;
	}
	if (class_letter == 'B') {
		session.device_class = RZI_LORAWAN_CLASS_B;
	} else if (class_letter == 'C') {
		session.device_class = RZI_LORAWAN_CLASS_C;
	} else {
		return -RZI_ERR_INVALID;
	}
	rc = parse_dev_addr(dev_addr, &session.dev_addr);
	if (rc == 0) {
		rc = rzi_at_lorawan_hex_to_bin(nwk_skey, session.network_session_key,
					       sizeof(session.network_session_key));
	}
	if (rc == 0) {
		rc = rzi_at_lorawan_hex_to_bin(app_skey, session.application_session_key,
					       sizeof(session.application_session_key));
	}
	if (rc != 0 || frequency_hz == 0U || data_rate > 15U) {
		return -RZI_ERR_INVALID;
	}
	session.frequency_hz = frequency_hz;
	session.data_rate = (enum rzi_lorawan_data_rate)data_rate;
	session.periodicity = (uint16_t)periodicity;
	rc = rzi_lorawan_add_multicast_session(&session);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_rmvmulc(const struct rzi_at_request *request, void *user_data)
{
	uint32_t dev_addr;
	int rc;

	ARG_UNUSED(user_data);
	rc = parse_dev_addr(request->argument, &dev_addr);
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_remove_multicast_session(dev_addr);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_lstmulc(const struct rzi_at_request *request, void *user_data)
{
	char line[CONFIG_RZI_AT_TX_BUFFER_SIZE];
	size_t count = 0;
	size_t used = 0;
	int rc;

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	rc = rzi_lorawan_get_multicast_count(&count);
	if (rc != 0) {
		return rc;
	}
	line[0] = '\0';
	for (size_t i = 0; i < count; ++i) {
		struct rzi_lorawan_multicast_session session;
		int written;

		rc = rzi_lorawan_get_multicast_session(i, &session);
		if (rc != 0) {
			return rc;
		}
		written = snprintf(line + used, sizeof(line) - used, "%sMC%u:%c:%08X:%u:%u:%u",
				   used == 0U ? "" : "\r\n", (unsigned int)(i + 1U),
				   session.device_class == RZI_LORAWAN_CLASS_B ? 'B' : 'C',
				   session.dev_addr, session.frequency_hz,
				   (unsigned int)session.data_rate, session.periodicity);
		if (written < 0 || (size_t)written >= sizeof(line) - used) {
			return -RZI_ERR_NO_RESOURCE;
		}
		used += (size_t)written;
	}
	if (used == 0U) {
		return rzi_at_respond_value("");
	}
	return rzi_at_respond_value("%s", line);
}

static const struct rzi_at_command commands[] = {
	{
		.name = "ADDMULC",
		.help = "add a new multicast group",
		.allowed_operations = RZI_AT_ALLOW_WRITE,
		.handler = handle_addmulc,
	},
	{
		.name = "RMVMULC",
		.help = "delete a multicast group",
		.allowed_operations = RZI_AT_ALLOW_WRITE,
		.handler = handle_rmvmulc,
	},
	{
		.name = "LSTMULC",
		.help = "view multicast group information",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_RUN,
		.handler = handle_lstmulc,
	},
};

const struct rzi_at_lorawan_command_group rzi_at_lorawan_multicast_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
