/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>

#include <rzi/at.h>

#include "../at_priv.h"

#define VERSION_STRING "RZI_0.1.0_" CONFIG_BOARD

static int version(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_value("AT+VER=%s", VERSION_STRING);
}

static const struct rzi_at_command commands[] = {
	{
		.name = "VER",
		.help = "get the version of the firmware",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = version,
	},
};

#if defined(CONFIG_RZI_AT_COMMAND_LORAWAN)
int rzi_at_lorawan_register(void);
#endif

int rzi_at_builtin_register(void)
{
	int rc = rzi_at_register(commands, ARRAY_SIZE(commands));

#if defined(CONFIG_RZI_AT_COMMAND_LORAWAN)
	if (rc == 0) {
		rc = rzi_at_lorawan_register();
	}
#endif
	return rc;
}
