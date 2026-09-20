/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Shared AT+NWM working-mode state and command.
 */

#include <errno.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>

#if defined(CONFIG_RZI_AT_NVM)
#include <rzi/storage/storage.h>
#endif

#include "../at_priv.h"
#include "at_network_mode.h"

#define LISTENER_MAX 2U

static uint8_t network_mode = IS_ENABLED(CONFIG_RZI_AT_COMMAND_LORAWAN)
				      ? RZI_AT_NETWORK_MODE_LORAWAN
				      : RZI_AT_NETWORK_MODE_P2P_LORA;
static rzi_at_network_mode_changed_t listeners[LISTENER_MAX];
static size_t listener_count;

static int parse_long(const char *argument, long *value)
{
	char *end = NULL;

	if (argument == NULL || value == NULL) {
		return -RZI_ERR_INVALID;
	}
	*value = strtol(argument, &end, 10);
	if (end == argument || *end != '\0') {
		return -RZI_ERR_INVALID;
	}
	return 0;
}

static void notify_listeners(uint8_t mode)
{
	for (size_t i = 0; i < listener_count; ++i) {
		listeners[i](mode);
	}
}

#if defined(CONFIG_RZI_AT_NVM)
static int load_mode(void)
{
	uint8_t stored = 0U;
	int rc = rzi_storage_init();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_storage_read("rzi", "nwm", &stored, sizeof(stored));
	if (rc == -RZI_ERR_NOT_FOUND) {
		return 0;
	}
	if (rc != 0) {
		return rc;
	}
	if (stored > RZI_AT_NETWORK_MODE_P2P_FSK) {
		return 0;
	}
	network_mode = stored;
	return 0;
}

static int save_mode(uint8_t mode)
{
	int rc = rzi_storage_init();

	if (rc != 0) {
		return rc;
	}
	return rzi_storage_write("rzi", "nwm", &mode, sizeof(mode));
}
#else
static int load_mode(void)
{
	return 0;
}

static int save_mode(uint8_t mode)
{
	ARG_UNUSED(mode);
	return 0;
}
#endif

uint8_t rzi_at_network_mode_get(void)
{
	return network_mode;
}

int rzi_at_network_mode_add_listener(rzi_at_network_mode_changed_t listener)
{
	if (listener == NULL) {
		return -RZI_ERR_INVALID;
	}
	if (listener_count >= ARRAY_SIZE(listeners)) {
		return -RZI_ERR_NO_RESOURCE;
	}
	listeners[listener_count++] = listener;
	return 0;
}

static int handle_nwm(const struct rzi_at_request *request, void *user_data)
{
	long parsed;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+NWM=%u", network_mode);
	}
	rc = parse_long(request->argument, &parsed);
	if (rc != 0 || parsed < 0 || parsed > (long)RZI_AT_NETWORK_MODE_P2P_FSK) {
		return -RZI_ERR_INVALID;
	}
	if ((uint8_t)parsed != RZI_AT_NETWORK_MODE_LORAWAN &&
	    !IS_ENABLED(CONFIG_RZI_AT_COMMAND_LORA)) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	if ((uint8_t)parsed == network_mode) {
		return rzi_at_respond_status(RZI_AT_STATUS_OK);
	}
	network_mode = (uint8_t)parsed;
	rc = save_mode(network_mode);
	if (rc != 0) {
		return rc;
	}
	notify_listeners(network_mode);
	rc = rzi_at_respond_status(RZI_AT_STATUS_OK);
	if (IS_ENABLED(CONFIG_RZI_AT_COMMAND_LORAWAN) && !IS_ENABLED(CONFIG_ZTEST)) {
		sys_reboot(SYS_REBOOT_COLD);
	}
	return rc;
}

static const struct rzi_at_command nwm_command = {
	.name = "NWM",
	.help = "get or set the network working mode (0 = P2P_LORA, 1 = LoRaWAN, 2 = P2P_FSK)",
	.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
	.handler = handle_nwm,
};

#if defined(CONFIG_RZI_AT_NVM)
static int extension_factory_reset(void)
{
	int rc = rzi_storage_delete("rzi", "nwm");

	return (rc == 0 || rc == -RZI_ERR_NOT_FOUND) ? 0 : rc;
}

static const struct rzi_at_extension extension = {
	.factory_reset = extension_factory_reset,
};
#endif

int rzi_at_network_mode_register(void)
{
	int rc = load_mode();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_at_register(&nwm_command, 1);
#if defined(CONFIG_RZI_AT_NVM)
	if (rc == 0) {
		rc = rzi_at_registry_add_extension(&extension);
	}
#endif
	return rc;
}
