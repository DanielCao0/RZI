/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN channel-mask and sub-band AT commands.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/sys/util.h>

#include "at_command_lorawan_priv.h"

static int handle_mask(const struct rzi_at_request *request, void *user_data)
{
	uint16_t mask[RZI_LORAWAN_CHANNEL_MASK_WORDS] = {0};
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = rzi_lorawan_get_channel_mask(mask, ARRAY_SIZE(mask));
		return rc != 0 ? rc : rzi_at_respond_value("AT+MASK=%04X", mask[0]);
	}
	if (strlen(request->argument) != 4U) {
		return -EINVAL;
	}

	char *end = NULL;
	unsigned long value = strtoul(request->argument, &end, 16);

	if (end == request->argument || *end != '\0' || value > 0xffffU) {
		return -EINVAL;
	}
	mask[0] = (uint16_t)value;
	rc = rzi_lorawan_set_channel_mask(mask, ARRAY_SIZE(mask));
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_che(const struct rzi_at_request *request, void *user_data)
{
	uint8_t sub_band;
	long parsed;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = rzi_lorawan_get_sub_band(&sub_band);
		return rc != 0 ? rc : rzi_at_respond_value("AT+CHE=%u", sub_band);
	}
	rc = rzi_at_lorawan_parse_long(request->argument, &parsed);
	if (rc != 0 || parsed < 0 || parsed > 8) {
		return -EINVAL;
	}
	rc = rzi_lorawan_set_sub_band((uint8_t)parsed);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_chs(const struct rzi_at_request *request, void *user_data)
{
	uint32_t frequency_hz;
	long parsed;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = rzi_lorawan_get_fixed_channel(&frequency_hz);
		return rc != 0 ? rc : rzi_at_respond_value("AT+CHS=%u", frequency_hz);
	}
	rc = rzi_at_lorawan_parse_long(request->argument, &parsed);
	if (rc != 0 || parsed < 0) {
		return -EINVAL;
	}
	rc = rzi_lorawan_set_fixed_channel((uint32_t)parsed);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static const struct rzi_at_command commands[] = {
	{
		.name = "MASK",
		.help = "get or set the channel mask (only for US915, AU915, CN470)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_mask,
	},
	{
		.name = "CHE",
		.help = "get or set eight channels mode (only for US915, AU915, CN470)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_che,
	},
	{
		.name = "CHS",
		.help = "get or set single channel mode (only for US915, AU915, CN470)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_chs,
	},
};

const struct rzi_at_lorawan_command_group rzi_at_lorawan_supplementary_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
