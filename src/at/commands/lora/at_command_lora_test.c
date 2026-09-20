/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RUI3 radio certification test AT commands.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/sys/util.h>

#include "at_command_lora_priv.h"

static int respond_ok(int rc)
{
	return rc == 0 ? rzi_at_respond_status(RZI_AT_STATUS_OK) : rc;
}

static int ensure_radio(void)
{
	return rzi_at_lora_ensure_started();
}

static int handle_trssi(const struct rzi_at_request *request, void *user_data)
{
	int rc;

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	rc = ensure_radio();
	if (rc != 0) {
		return rc;
	}
	return respond_ok(rzi_lora_test_rssi());
}

static int handle_ttone(const struct rzi_at_request *request, void *user_data)
{
	int rc;

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	rc = ensure_radio();
	if (rc != 0) {
		return rc;
	}
	return respond_ok(rzi_lora_test_tone());
}

static int handle_count(const struct rzi_at_request *request, int (*fn)(uint32_t))
{
	long parsed;
	int rc;

	rc = rzi_at_lora_parse_long(request->argument, &parsed);
	if (rc != 0 || parsed < 0) {
		return -RZI_ERR_INVALID;
	}
	rc = ensure_radio();
	if (rc != 0) {
		return rc;
	}
	return respond_ok(fn((uint32_t)parsed));
}

static int handle_ttx(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_count(request, rzi_lora_test_tx);
}

static int handle_trx(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_count(request, rzi_lora_test_rx);
}

static int handle_tconf(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_lora_config config;
	const char *cursor;
	char *end;
	unsigned long values[6];
	size_t count = 0;
	int rc;

	ARG_UNUSED(user_data);
	rc = rzi_lora_get_config(&config);
	if (rc != 0) {
		return rc;
	}
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+TCONF=%u:%u:%u:%u:%u:%d", config.frequency_hz,
					    config.spreading_factor, config.bandwidth,
					    config.coding_rate, config.preamble_length,
					    config.tx_power_dbm);
	}
	cursor = request->argument;
	while (*cursor != '\0' && count < ARRAY_SIZE(values)) {
		values[count] = strtoul(cursor, &end, 10);
		if (end == cursor) {
			return -RZI_ERR_INVALID;
		}
		++count;
		if (*end == '\0') {
			break;
		}
		if (*end != ':') {
			return -RZI_ERR_INVALID;
		}
		cursor = end + 1;
	}
	if (count < 6U) {
		return -RZI_ERR_INVALID;
	}
	config.frequency_hz = (uint32_t)values[0];
	config.spreading_factor = (uint8_t)values[1];
	config.bandwidth = (uint32_t)values[2];
	config.coding_rate = (uint8_t)values[3];
	config.preamble_length = (uint16_t)values[4];
	config.tx_power_dbm = (int8_t)values[5];
	return respond_ok(rzi_lora_set_config(&config));
}

static int handle_hop(const struct rzi_at_request *request, int (*fn)(uint32_t))
{
	const char *cursor = request->argument;
	char *end;
	unsigned long values[4];
	size_t count = 0;
	int rc;

	while (*cursor != '\0' && count < ARRAY_SIZE(values)) {
		values[count] = strtoul(cursor, &end, 10);
		if (end == cursor) {
			return -RZI_ERR_INVALID;
		}
		++count;
		if (*end == '\0') {
			break;
		}
		if (*end != ':') {
			return -RZI_ERR_INVALID;
		}
		cursor = end + 1;
	}
	if (count != 4U) {
		return -RZI_ERR_INVALID;
	}
	rc = ensure_radio();
	if (rc != 0) {
		return rc;
	}
	return respond_ok(fn((uint32_t)values[3]));
}

static int handle_tth(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_hop(request, rzi_lora_test_tx);
}

static int handle_trth(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_hop(request, rzi_lora_test_rx);
}

static int handle_toff(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return respond_ok(rzi_lora_test_stop());
}

static int handle_cw(const struct rzi_at_request *request, void *user_data)
{
	char *end;
	const char *cursor = request->argument;
	unsigned long frequency;
	long power;
	unsigned long duration;
	int rc;

	ARG_UNUSED(user_data);
	frequency = strtoul(cursor, &end, 10);
	if (end == cursor || *end != ':') {
		return -RZI_ERR_INVALID;
	}
	cursor = end + 1;
	power = strtol(cursor, &end, 10);
	if (end == cursor || *end != ':') {
		return -RZI_ERR_INVALID;
	}
	cursor = end + 1;
	duration = strtoul(cursor, &end, 10);
	if (end == cursor || *end != '\0') {
		return -RZI_ERR_INVALID;
	}
	rc = ensure_radio();
	if (rc != 0) {
		return rc;
	}
	return respond_ok(rzi_lora_test_cw((uint32_t)frequency, (int8_t)power, (uint32_t)duration));
}

static const struct rzi_at_command commands[] = {
	{
		.name = "TRSSI",
		.help = "start RF RSSI tone test",
		.allowed_operations = RZI_AT_ALLOW_RUN,
		.handler = handle_trssi,
	},
	{
		.name = "TTONE",
		.help = "start RF CW tone test",
		.allowed_operations = RZI_AT_ALLOW_RUN,
		.handler = handle_ttone,
	},
	{
		.name = "TTX",
		.help = "start RF TX test, set the number of packets",
		.allowed_operations = RZI_AT_ALLOW_WRITE,
		.handler = handle_ttx,
	},
	{
		.name = "TRX",
		.help = "start RF RX test, set the number of packets",
		.allowed_operations = RZI_AT_ALLOW_WRITE,
		.handler = handle_trx,
	},
	{
		.name = "TCONF",
		.help = "configure LoRa RF test (Freq:SF:BW:CR:PPL:PTP)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_tconf,
	},
	{
		.name = "TTH",
		.help = "start RF TX hopping test (start:stop:hop:count)",
		.allowed_operations = RZI_AT_ALLOW_WRITE,
		.handler = handle_tth,
	},
	{
		.name = "TRTH",
		.help = "start RF RX hopping test (start:stop:hop:count)",
		.allowed_operations = RZI_AT_ALLOW_WRITE,
		.handler = handle_trth,
	},
	{
		.name = "TOFF",
		.help = "stop an ongoing RF test",
		.allowed_operations = RZI_AT_ALLOW_RUN,
		.handler = handle_toff,
	},
	{
		.name = "CW",
		.help = "start continuous wave (Freq:Power:Duration)",
		.allowed_operations = RZI_AT_ALLOW_WRITE,
		.handler = handle_cw,
	},
};

const struct rzi_at_lora_command_group rzi_at_lora_test_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
