/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN certification-mode AT command.
 */

#include <errno.h>

#include <zephyr/sys/util.h>

#include <rzi/lorawan/certification.h>

#include "at_command_lorawan_priv.h"

static int handle_certif(const struct rzi_at_request *request, void *user_data)
{
	bool enabled;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = rzi_lorawan_get_certification_mode(&enabled);
		return rc != 0 ? rc : rzi_at_respond_value("AT+CERTIF=%d", enabled ? 1 : 0);
	}
	rc = rzi_at_lorawan_parse_bool(request->argument, &enabled);
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_set_certification_mode(enabled);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static const struct rzi_at_command commands[] = {
	{
		.name = "CERTIF",
		.help = "set the module in LoRaWAN certification mode (0 = normal, 1 = "
			"certification)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_certif,
	},
};

const struct rzi_at_lorawan_command_group rzi_at_lorawan_certification_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
