/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN channel-mask and sub-band AT commands.
 *
 * RUI3 groups the US915 / AU915 channels into eight 125 kHz blocks of eight
 * channels and one group with the eight 500 kHz channels 64-71.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/sys/util.h>

#include "at_lorawan_priv.h"

#define NARROW_BLOCKS 8U
#define WIDE_GROUP    NARROW_BLOCKS
#define GROUPS        (NARROW_BLOCKS + 1U)
#define WIDE_FIRST    64U
#define MASK_ALL      BIT_MASK(GROUPS)

static bool has_channel(const uint16_t *mask, uint32_t channel)
{
	return (mask[channel / 16U] & BIT(channel % 16U)) != 0U;
}

static void put_channel(uint16_t *mask, uint32_t channel)
{
	mask[channel / 16U] |= BIT(channel % 16U);
}

static uint32_t group_first(uint32_t group)
{
	return group == WIDE_GROUP ? WIDE_FIRST : group * 8U;
}

static void put_group(uint16_t *mask, uint32_t group)
{
	uint32_t first = group_first(group);

	for (uint32_t channel = first; channel < first + 8U; ++channel) {
		put_channel(mask, channel);
	}
}

static uint32_t enabled_in_group(const uint16_t *mask, uint32_t group)
{
	uint32_t first = group_first(group);
	uint32_t enabled = 0U;

	for (uint32_t channel = first; channel < first + 8U; ++channel) {
		enabled += has_channel(mask, channel) ? 1U : 0U;
	}
	return enabled;
}

/* Bit g is group g: fully enabled groups, or partly enabled ones if none is full. */
static int read_groups(uint16_t *groups)
{
	uint16_t mask[RZI_LORAWAN_CHANNEL_MASK_WORDS];
	uint16_t full = 0U;
	uint16_t partial = 0U;
	int rc = rzi_lorawan_get_channel_mask(mask, ARRAY_SIZE(mask));

	if (rc != 0) {
		return rc;
	}
	for (uint32_t group = 0; group < GROUPS; ++group) {
		uint32_t enabled = enabled_in_group(mask, group);

		if (enabled == 8U) {
			full |= BIT(group);
		}
		if (enabled != 0U) {
			partial |= BIT(group);
		}
	}
	*groups = full != 0U ? full : partial;
	return 0;
}

static int write_mask(const uint16_t *mask)
{
	int rc = rzi_lorawan_set_channel_mask(mask, RZI_LORAWAN_CHANNEL_MASK_WORDS);

	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

/* AT+MASK bit b (0-7) enables block b and 500 kHz channel 64+b; bit 8 all 500 kHz channels. */
static int handle_mask(const struct rzi_at_request *request, void *user_data)
{
	uint16_t mask[RZI_LORAWAN_CHANNEL_MASK_WORDS] = {0};
	uint16_t groups;
	char *end = NULL;
	unsigned long value;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = read_groups(&groups);
		return rc != 0 ? rc : rzi_at_respond_value("AT+MASK=%04X", groups);
	}
	if (strlen(request->argument) != 4U) {
		return -RZI_ERR_INVALID;
	}
	value = strtoul(request->argument, &end, 16);
	if (end == request->argument || *end != '\0' || (value & ~MASK_ALL) != 0U) {
		return -RZI_ERR_INVALID;
	}
	if (value == 0U) {
		value = MASK_ALL;
	}
	for (uint32_t block = 0; block < NARROW_BLOCKS; ++block) {
		if ((value & BIT(block)) != 0U) {
			put_group(mask, block);
			put_channel(mask, WIDE_FIRST + block);
		}
	}
	if ((value & BIT(WIDE_GROUP)) != 0U) {
		put_group(mask, WIDE_GROUP);
	}
	return write_mask(mask);
}

/* AT+CHE=n[:n...]: 1-8 is a 125 kHz block, 9 the 500 kHz channels, 0 every channel. */
static int handle_che(const struct rzi_at_request *request, void *user_data)
{
	uint16_t mask[RZI_LORAWAN_CHANNEL_MASK_WORDS] = {0};
	char list[GROUPS * 2U + 1U];
	const char *cursor = request->argument;
	uint16_t groups;
	size_t used = 0U;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = read_groups(&groups);
		if (rc != 0) {
			return rc;
		}
		if (groups == MASK_ALL) {
			return rzi_at_respond_value("AT+CHE=0");
		}
		list[0] = '\0';
		for (uint32_t group = 0; group < GROUPS; ++group) {
			if ((groups & BIT(group)) != 0U) {
				used += (size_t)snprintf(&list[used], sizeof(list) - used, "%s%u",
							 used == 0U ? "" : ":", group + 1U);
			}
		}
		return rzi_at_respond_value("AT+CHE=%s", list);
	}
	if (strcmp(request->argument, "0") == 0) {
		put_group(mask, WIDE_GROUP);
		for (uint32_t block = 0; block < NARROW_BLOCKS; ++block) {
			put_group(mask, block);
		}
		return write_mask(mask);
	}
	for (size_t count = 0;; ++count) {
		char *end = NULL;
		unsigned long value = strtoul(cursor, &end, 10);

		if (count >= GROUPS || end == cursor || value < 1U || value > GROUPS ||
		    (*end != ':' && *end != '\0')) {
			return -RZI_ERR_INVALID;
		}
		put_group(mask, value - 1U);
		if (*end == '\0') {
			break;
		}
		cursor = end + 1;
	}
	return write_mask(mask);
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
		return -RZI_ERR_INVALID;
	}
	rc = rzi_lorawan_set_fixed_channel((uint32_t)parsed);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static const struct rzi_at_command commands[] = {
	{
		.name = "MASK",
		.help = "get or set the channel mask (only for US915, AU915)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_mask,
	},
	{
		.name = "CHE",
		.help = "get or set eight channels mode (only for US915, AU915)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_che,
	},
	{
		.name = "CHS",
		.help = "get or set single channel mode (only for US915, AU915)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_chs,
	},
};

const struct rzi_at_lorawan_command_group rzi_at_lorawan_supplementary_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
