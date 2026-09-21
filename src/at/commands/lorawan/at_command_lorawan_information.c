/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN RSSI and SNR AT commands.
 */

#include <zephyr/sys/util.h>

#include "at_command_lorawan_priv.h"

static int handle_rssi(const struct rzi_at_request *request, void *user_data)
{
	int16_t rssi;
	int rc;

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	rc = rzi_lorawan_get_last_rssi(&rssi);
	return rc != 0 ? rc : rzi_at_respond_value("AT+RSSI=%d", rssi);
}

static int handle_snr(const struct rzi_at_request *request, void *user_data)
{
	int8_t snr;
	int rc;

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	rc = rzi_lorawan_get_last_snr(&snr);
	return rc != 0 ? rc : rzi_at_respond_value("AT+SNR=%d", snr / 4);
}

static const struct rzi_at_command commands[] = {
	{
		.name = "RSSI",
		.help = "get the RSSI of the last received packet",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_rssi,
	},
	{
		.name = "SNR",
		.help = "get the SNR of the last received packet",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_snr,
	},
};

const struct rzi_at_lorawan_command_group rzi_at_lorawan_information_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
