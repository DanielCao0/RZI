/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN region, class, and MAC-parameter AT commands.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/sys/util.h>

#include <rzi/lorawan/mac_commands.h>

#include "at_command_lorawan_priv.h"

static int class_supported(enum rzi_lorawan_class device_class)
{
	uint32_t capabilities = rzi_lorawan_get_capabilities();

	switch (device_class) {
	case RZI_LORAWAN_CLASS_A:
		return (capabilities & RZI_LORAWAN_CAP_CLASS_A) != 0U ? 0 : -RZI_ERR_NOT_SUPPORTED;
	case RZI_LORAWAN_CLASS_B:
		return (capabilities & RZI_LORAWAN_CAP_CLASS_B) != 0U ? 0 : -RZI_ERR_NOT_SUPPORTED;
	case RZI_LORAWAN_CLASS_C:
		return (capabilities & RZI_LORAWAN_CAP_CLASS_C) != 0U ? 0 : -RZI_ERR_NOT_SUPPORTED;
	default:
		return -RZI_ERR_INVALID;
	}
}

static char class_letter(enum rzi_lorawan_class device_class)
{
	switch (device_class) {
	case RZI_LORAWAN_CLASS_B:
		return 'B';
	case RZI_LORAWAN_CLASS_C:
		return 'C';
	default:
		return 'A';
	}
}

static int handle_bool(const struct rzi_at_request *request, const char *name, int (*get)(bool *),
		       int (*set)(bool))
{
	bool enabled;
	int rc;

	if (request->operation == RZI_AT_OP_READ) {
		rc = get(&enabled);
		return rc != 0 ? rc : rzi_at_respond_value("AT+%s=%d", name, enabled ? 1 : 0);
	}
	rc = rzi_at_lorawan_parse_bool(request->argument, &enabled);
	if (rc != 0) {
		return rc;
	}
	rc = set(enabled);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_u32(const struct rzi_at_request *request, const char *name,
		      int (*get)(uint32_t *), int (*set)(uint32_t), uint32_t scale)
{
	uint32_t value;
	long parsed;
	int rc;

	if (request->operation == RZI_AT_OP_READ) {
		rc = get(&value);
		return rc != 0 ? rc : rzi_at_respond_value("AT+%s=%u", name, value / scale);
	}
	if (set == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	rc = rzi_at_lorawan_parse_long(request->argument, &parsed);
	if (rc != 0 || parsed <= 0) {
		return -RZI_ERR_INVALID;
	}
	rc = set((uint32_t)parsed * scale);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
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
		return -RZI_ERR_BUSY;
	}

	char *end = NULL;
	long band = strtol(request->argument, &end, 10);
	enum rzi_lorawan_region region;
	int rc;

	if (end == request->argument || *end != '\0') {
		return -RZI_ERR_INVALID;
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
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	enum rzi_lorawan_class device_class;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		if (context->service_started) {
			rc = rzi_lorawan_get_class(&device_class);
			if (rc != 0) {
				return rc;
			}
			context->device_class = device_class;
		}
		return rzi_at_respond_value("AT+CLASS=%c", class_letter(context->device_class));
	}
	if (strcmp(request->argument, "A") == 0) {
		device_class = RZI_LORAWAN_CLASS_A;
	} else if (strcmp(request->argument, "B") == 0) {
		device_class = RZI_LORAWAN_CLASS_B;
	} else if (strcmp(request->argument, "C") == 0) {
		device_class = RZI_LORAWAN_CLASS_C;
	} else {
		return -RZI_ERR_INVALID;
	}
	rc = class_supported(device_class);
	if (rc != 0) {
		return rc;
	}
	if (context->service_started) {
		rc = rzi_lorawan_set_class(device_class);
		if (rc != 0) {
			return rc;
		}
	}
	context->device_class = device_class;
	return rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_adr(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_bool(request, "ADR", rzi_lorawan_get_adr, rzi_lorawan_set_adr);
}

static int handle_dcs(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_bool(request, "DCS", rzi_lorawan_get_duty_cycle, rzi_lorawan_set_duty_cycle);
}

static int handle_pnm(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_bool(request, "PNM", rzi_lorawan_get_public_network,
			   rzi_lorawan_set_public_network);
}

static int handle_lbt(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_bool(request, "LBT", rzi_lorawan_get_lbt, rzi_lorawan_set_lbt);
}

static int handle_dr(const struct rzi_at_request *request, void *user_data)
{
	enum rzi_lorawan_data_rate data_rate;
	long parsed;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = rzi_lorawan_get_data_rate(&data_rate);
		return rc != 0 ? rc : rzi_at_respond_value("AT+DR=%u", (unsigned int)data_rate);
	}
	rc = rzi_at_lorawan_parse_long(request->argument, &parsed);
	if (rc != 0 || parsed < 0 || parsed > 15) {
		return -RZI_ERR_INVALID;
	}
	rc = rzi_lorawan_set_data_rate((enum rzi_lorawan_data_rate)parsed);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_txp(const struct rzi_at_request *request, void *user_data)
{
	uint8_t tx_power;
	long parsed;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = rzi_lorawan_get_tx_power(&tx_power);
		return rc != 0 ? rc : rzi_at_respond_value("AT+TXP=%u", tx_power);
	}
	rc = rzi_at_lorawan_parse_long(request->argument, &parsed);
	if (rc != 0 || parsed < 0 || parsed > 15) {
		return -RZI_ERR_INVALID;
	}
	rc = rzi_lorawan_set_tx_power((uint8_t)parsed);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_rx1dl(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_u32(request, "RX1DL", rzi_lorawan_get_rx1_delay, rzi_lorawan_set_rx1_delay,
			  1000U);
}

static int handle_rx2dl(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_u32(request, "RX2DL", rzi_lorawan_get_rx2_delay, rzi_lorawan_set_rx2_delay,
			  1000U);
}

static int handle_jn1dl(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_u32(request, "JN1DL", rzi_lorawan_get_join_accept_delay1,
			  rzi_lorawan_set_join_accept_delay1, 1000U);
}

static int handle_jn2dl(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_u32(request, "JN2DL", rzi_lorawan_get_join_accept_delay2,
			  rzi_lorawan_set_join_accept_delay2, 1000U);
}

static int handle_rx2dr(const struct rzi_at_request *request, void *user_data)
{
	enum rzi_lorawan_data_rate data_rate;
	long parsed;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = rzi_lorawan_get_rx2_data_rate(&data_rate);
		return rc != 0 ? rc : rzi_at_respond_value("AT+RX2DR=%u", (unsigned int)data_rate);
	}
	rc = rzi_at_lorawan_parse_long(request->argument, &parsed);
	if (rc != 0 || parsed < 0 || parsed > 15) {
		return -RZI_ERR_INVALID;
	}
	rc = rzi_lorawan_set_rx2_data_rate((enum rzi_lorawan_data_rate)parsed);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_rx2fq(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_u32(request, "RX2FQ", rzi_lorawan_get_rx2_frequency,
			  rzi_lorawan_set_rx2_frequency, 1U);
}

static int handle_lbt_rssi(const struct rzi_at_request *request, void *user_data)
{
	int16_t rssi;
	long parsed;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = rzi_lorawan_get_lbt_rssi(&rssi);
		return rc != 0 ? rc : rzi_at_respond_value("AT+LBTRSSI=%d", rssi);
	}
	rc = rzi_at_lorawan_parse_long(request->argument, &parsed);
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_set_lbt_rssi((int16_t)parsed);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_lbt_scan(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return handle_u32(request, "LBTSCANTIME", rzi_lorawan_get_lbt_scan_time,
			  rzi_lorawan_set_lbt_scan_time, 1U);
}

static int handle_timereq(const struct rzi_at_request *request, void *user_data)
{
	bool enabled;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = rzi_lorawan_get_device_time_enabled(&enabled);
		return rc != 0 ? rc : rzi_at_respond_value("AT+TIMEREQ=%d", enabled ? 1 : 0);
	}
	rc = rzi_at_lorawan_parse_bool(request->argument, &enabled);
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_request_device_time(enabled);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_linkcheck(const struct rzi_at_request *request, void *user_data)
{
	enum rzi_lorawan_link_check_mode mode;
	long parsed;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = rzi_lorawan_get_link_check_mode(&mode);
		return rc != 0 ? rc : rzi_at_respond_value("AT+LINKCHECK=%u", (unsigned int)mode);
	}
	rc = rzi_at_lorawan_parse_long(request->argument, &parsed);
	if (rc != 0 || parsed < 0 || parsed > 2) {
		return -RZI_ERR_INVALID;
	}
	rc = rzi_lorawan_request_link_check((enum rzi_lorawan_link_check_mode)parsed);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static const struct rzi_at_command commands[] = {
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
	{
		.name = "ADR",
		.help = "get or set the adaptive data rate setting (0 = off, 1 = on)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_adr,
	},
	{
		.name = "DCS",
		.help = "get or set the ETSI duty cycle setting (0 = disabled, 1 = enabled)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_dcs,
	},
	{
		.name = "DR",
		.help = "get or set the data rate",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_dr,
	},
	{
		.name = "TXP",
		.help = "get or set the transmitting power",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_txp,
	},
	{
		.name = "PNM",
		.help = "get or set the public network mode (0 = off, 1 = on)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_pnm,
	},
	{
		.name = "RX1DL",
		.help = "get or set the delay between the end of TX and RX window 1 in seconds",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_rx1dl,
	},
	{
		.name = "RX2DL",
		.help = "get or set the delay between the end of TX and RX window 2 in seconds",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_rx2dl,
	},
	{
		.name = "RX2DR",
		.help = "get or set the RX2 window data rate",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_rx2dr,
	},
	{
		.name = "RX2FQ",
		.help = "get or set the RX2 window frequency (Hz)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_rx2fq,
	},
	{
		.name = "JN1DL",
		.help = "get or set the join accept delay for Rx window 1 in seconds",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_jn1dl,
	},
	{
		.name = "JN2DL",
		.help = "get or set the join accept delay for Rx window 2 in seconds",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_jn2dl,
	},
	{
		.name = "LBT",
		.help = "get or set LoRaWAN LBT (0 = disabled, 1 = enabled)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_lbt,
	},
	{
		.name = "LBTRSSI",
		.help = "get or set the LoRaWAN LBT RSSI threshold",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_lbt_rssi,
	},
	{
		.name = "LBTSCANTIME",
		.help = "get or set the LoRaWAN LBT scan time in milliseconds",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_lbt_scan,
	},
	{
		.name = "LINKCHECK",
		.help = "get or set the link check setting (0 = disabled, 1 = once, 2 = everytime)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_linkcheck,
	},
	{
		.name = "TIMEREQ",
		.help = "request the current date and time (0 = disabled, 1 = enabled)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_timereq,
	},
};

const struct rzi_at_lorawan_command_group rzi_at_lorawan_network_management_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
