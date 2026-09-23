/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN Class B AT commands.
 */

#include <errno.h>

#include <zephyr/sys/util.h>

#include "at_lorawan_priv.h"

static int handle_pgslot(const struct rzi_at_request *request, void *user_data)
{
	uint8_t periodicity;
	long parsed;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = rzi_lorawan_get_ping_slot_periodicity(&periodicity);
		return rc != 0 ? rc : rzi_at_respond_value("AT+PGSLOT=%u", periodicity);
	}
	rc = rzi_at_lorawan_parse_long(request->argument, &parsed);
	if (rc != 0 || parsed < 0 || parsed > 7) {
		return -RZI_ERR_INVALID;
	}
	rc = rzi_lorawan_set_ping_slot_periodicity((uint8_t)parsed);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_bfreq(const struct rzi_at_request *request, void *user_data)
{
	enum rzi_lorawan_data_rate data_rate;
	uint32_t frequency_hz;
	int rc;

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	rc = rzi_lorawan_get_beacon_data_rate(&data_rate);
	if (rc == 0) {
		rc = rzi_lorawan_get_beacon_frequency(&frequency_hz);
	}
	if (rc != 0) {
		return rc;
	}
	return rzi_at_respond_value("BCON: %u, %u.%03u", (unsigned int)data_rate,
				    frequency_hz / 1000000U, (frequency_hz / 1000U) % 1000U);
}

static int handle_btime(const struct rzi_at_request *request, void *user_data)
{
	uint32_t gps_time;
	int rc;

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	rc = rzi_lorawan_get_beacon_time(&gps_time);
	return rc != 0 ? rc : rzi_at_respond_value("BTIME: %u", gps_time);
}

static int handle_bgw(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_lorawan_beacon_gateway gateway;
	int rc;

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	rc = rzi_lorawan_get_beacon_gateway(&gateway);
	if (rc != 0) {
		return rc;
	}
	return rzi_at_respond_value("BGW: %d, %u, %u, %u, %u", gateway.has_coordinates ? 1 : 0,
				    gateway.net_id, gateway.gateway_id, gateway.longitude,
				    gateway.latitude);
}

static int handle_ltime(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_lorawan_network_time time;
	int rc;

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	rc = rzi_lorawan_get_network_time(&time);
	return rc != 0 ? rc : rzi_at_respond_value("LTIME: GPS %u", time.gps_seconds);
}

static const struct rzi_at_command commands[] = {
	{
		.name = "PGSLOT",
		.help = "get or set the unicast ping slot periodicity (0-7)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_pgslot,
	},
	{
		.name = "BFREQ",
		.help = "get the data rate and beacon frequency",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_bfreq,
	},
	{
		.name = "BTIME",
		.help = "get the beacon time (seconds since GPS Epoch time)",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_btime,
	},
	{
		.name = "BGW",
		.help = "get the gateway GPS coordinate, NetID and GwID",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_bgw,
	},
	{
		.name = "LTIME",
		.help = "get the local time from DeviceTimeAns",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_ltime,
	},
};

const struct rzi_at_lorawan_command_group rzi_at_lorawan_class_b_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
