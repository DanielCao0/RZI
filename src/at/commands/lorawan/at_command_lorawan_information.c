/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN RSSI, SNR, and channel-scan AT commands.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/sys/util.h>

#include <rzi/lorawan/channel_scan.h>

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

static int handle_arssi(const struct rzi_at_request *request, void *user_data)
{
	char line[CONFIG_RZI_AT_TX_BUFFER_SIZE];
	size_t count = 0;
	size_t used = 0;
	int rc;

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	rc = rzi_lorawan_get_channel_rssi_count(&count);
	if (rc != 0) {
		return rc;
	}
	line[0] = '\0';
	for (size_t i = 0; i < count; ++i) {
		struct rzi_lorawan_channel_rssi sample;
		int written;

		rc = rzi_lorawan_get_channel_rssi(i, &sample);
		if (rc != 0) {
			return rc;
		}
		written = snprintf(line + used, sizeof(line) - used, "%s%u,%d",
				   used == 0U ? "AT+ARSSI=" : " ", sample.channel, sample.rssi_dbm);
		if (written < 0 || (size_t)written >= sizeof(line) - used) {
			return -ENOMEM;
		}
		used += (size_t)written;
	}
	if (used == 0U) {
		return rzi_at_respond_value("AT+ARSSI=");
	}
	return rzi_at_respond_value("%s", line);
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
	{
		.name = "ARSSI",
		.help = "access all open channel RSSI",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_arssi,
	},
};

const struct rzi_at_lorawan_command_group rzi_at_lorawan_information_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
