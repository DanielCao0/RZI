/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN network mode, region, and class AT commands.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/sys/util.h>

#include "at_command_lorawan_priv.h"

static int handle_nwm(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);

	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+NWM=1");
	}
	return strcmp(request->argument, "1") == 0 ? rzi_at_respond_status(RZI_AT_STATUS_OK)
						   : -EINVAL;
}

static int handle_band(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+BAND=%d",
					    rzi_at_lorawan_region_to_band(context->region));
	}
	if (context->service_started) {
		return -EBUSY;
	}

	char *end = NULL;
	long band = strtol(request->argument, &end, 10);
	enum rzi_lorawan_region region;
	int rc;

	if (end == request->argument || *end != '\0') {
		return -EINVAL;
	}
	rc = rzi_at_lorawan_band_to_region((int)band, &region);
	if (rc != 0) {
		return rc;
	}

	uint8_t stored = (uint8_t)band;

	context->region = region;
	rc = rzi_at_lorawan_nvm_save("band", &stored, sizeof(stored));
	if (rc != 0) {
		return rc;
	}
	return rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_class(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);

	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+CLASS=A");
	}
	/* The current RZI backend contract exposes Class A only. */
	return strcmp(request->argument, "A") == 0 ? rzi_at_respond_status(RZI_AT_STATUS_OK)
						   : -EINVAL;
}

static const struct rzi_at_command commands[] = {
	{
		.name = "NWM",
		.help = "get or set the network work mode (0 = P2P, 1 = LoRaWAN)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_nwm,
	},
	{
		.name = "BAND",
		.help = "get or set the active region (0 = EU433, 1 = CN470, 2 = RU864, "
			"3 = IN865, 4 = EU868, 5 = US915, 6 = AU915, 7 = KR920, "
			"8 = AS923-1, 9 = AS923-2, 10 = AS923-3, 11 = AS923-4, "
			"12 = LA915)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_band,
	},
	{
		.name = "CLASS",
		.help = "get or set the device class (A = class A, B = class B, C = class C)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_class,
	},
};

const struct rzi_at_lorawan_command_group rzi_at_lorawan_network_management_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
