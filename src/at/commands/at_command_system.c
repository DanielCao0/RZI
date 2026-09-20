/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Built-in system AT commands and optional command-package registration.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>

#if defined(CONFIG_HWINFO)
#include <zephyr/drivers/hwinfo.h>
#endif
#include <rzi/version.h>

#if defined(CONFIG_RZI_AT_ADAPTER_UART)
#include <rzi/at/uart.h>
#endif
#if defined(CONFIG_RZI_AT_NVM)
#include <rzi/storage/storage.h>
#endif
#if defined(CONFIG_RZI_POWER)
#include <rzi/power/power.h>
#endif
#if defined(CONFIG_RZI_RSUP)
#include <rzi/rsup/rsup.h>
#endif
#if defined(CONFIG_BT)
#include <zephyr/bluetooth/bluetooth.h>
#endif

#include "../at_priv.h"

static __maybe_unused void ignore_result(int result)
{
	ARG_UNUSED(result);
}

#if defined(CONFIG_RZI_AT_COMMAND_LORAWAN)
int rzi_at_lorawan_register(void);
#endif
#if defined(CONFIG_RZI_AT_COMMAND_LORA)
int rzi_at_lora_register(void);
#endif

#define VERSION_STRING "RZI_" RZI_VERSION_STRING "_" CONFIG_BOARD
#define ALIAS_MAX      16U
#define SN_MAX         18U

static char alias_name[ALIAS_MAX + 1U];
static char serial_number[SN_MAX + 1U];
static uint8_t lpm_enabled;
static uint8_t lpm_level = 1U;

static int load_string(const char *key, char *dst, size_t max_len)
{
#if defined(CONFIG_RZI_AT_NVM)
	char stored[ALIAS_MAX + 1U];
	int rc = rzi_storage_read("rzi", key, stored, sizeof(stored));

	if (rc == 0) {
		stored[sizeof(stored) - 1U] = '\0';
		strncpy(dst, stored, max_len);
		dst[max_len] = '\0';
	}
	return rc == -ENOENT ? 0 : rc;
#else
	ARG_UNUSED(key);
	ARG_UNUSED(dst);
	ARG_UNUSED(max_len);
	return 0;
#endif
}

static int save_string(const char *key, const char *value, size_t max_len)
{
#if defined(CONFIG_RZI_AT_NVM)
	char stored[ALIAS_MAX + 1U] = {0};

	strncpy(stored, value, max_len);
	return rzi_storage_write("rzi", key, stored, sizeof(stored));
#else
	ARG_UNUSED(key);
	ARG_UNUSED(value);
	ARG_UNUSED(max_len);
	return 0;
#endif
}

static void fill_serial_from_hwinfo(void)
{
	uint8_t id[8] = {0};
	static const char digits[] = "0123456789ABCDEF";
	ssize_t n = -ENOTSUP;
	size_t count;

#if defined(CONFIG_HWINFO)
	n = hwinfo_get_device_id(id, sizeof(id));
#endif
	count = n > 0 ? MIN((size_t)n, 9U) : 0U;

	if (count == 0U) {
		strncpy(serial_number, CONFIG_BOARD, SN_MAX);
		serial_number[SN_MAX] = '\0';
		return;
	}
	for (size_t i = 0; i < count; ++i) {
		serial_number[i * 2U] = digits[id[i] >> 4];
		serial_number[i * 2U + 1U] = digits[id[i] & 0x0f];
	}
	serial_number[count * 2U] = '\0';
}

static int handle_version(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_value("AT+VER=%s", VERSION_STRING);
}

static int handle_firmwarever(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_value("AT+FIRMWAREVER=%s", VERSION_STRING);
}

static int handle_cliver(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_value("AT+CLIVER=%s", RZI_VERSION_STRING);
}

static int handle_apiver(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_value("AT+APIVER=%s", RZI_VERSION_STRING);
}

static int handle_hwmodel(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_value("AT+HWMODEL=%s", CONFIG_BOARD);
}

static int handle_hwid(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_value("AT+HWID=%s", CONFIG_SOC);
}

static int handle_sn(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	if (serial_number[0] == '\0') {
		fill_serial_from_hwinfo();
	}
	return rzi_at_respond_value("AT+SN=%s", serial_number);
}

static int handle_alias(const struct rzi_at_request *request, void *user_data)
{
	size_t len;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+ALIAS=%s", alias_name);
	}
	len = strlen(request->argument);
	if (len == 0U || len > ALIAS_MAX) {
		return -EINVAL;
	}
	memcpy(alias_name, request->argument, len + 1U);
	rc = save_string("alias", alias_name, ALIAS_MAX);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_buildtime(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_value("AT+BUILDTIME=%s-%s", __DATE__, __TIME__);
}

static int handle_repoinfo(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_value("AT+REPOINFO=%s:0:0:0:0:0:0:0", RZI_VERSION_STRING);
}

static int handle_boot(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	ignore_result(rzi_at_respond_status(RZI_AT_STATUS_OK));
#if defined(CONFIG_RZI_RSUP)
	rzi_rsup_arm_reboot();
#else
	sys_reboot(SYS_REBOOT_COLD);
#endif
	return 0;
}

static int handle_bootver(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_value("AT+BOOTVER=RZI MCUboot");
}

static int handle_debug(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_value("AT+DEBUG=%s locked=%d echo=%d", VERSION_STRING,
				    rzi_at_is_locked() ? 1 : 0, rzi_at_echo_enabled() ? 1 : 0);
}

static int handle_factory(const struct rzi_at_request *request, void *user_data)
{
	int rc;

	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	rc = rzi_at_registry_factory_reset();
	if (rc != 0) {
		return rc;
	}
	ignore_result(rzi_at_respond_status(RZI_AT_STATUS_OK));
	sys_reboot(SYS_REBOOT_COLD);
	return 0;
}

static int handle_lock(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+LOCK=%s",
					    rzi_at_is_locked() ? "locked" : "unlocked");
	}
	rzi_at_set_locked(true);
	return rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_pword(const struct rzi_at_request *request, void *user_data)
{
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+PWORD=%s", rzi_at_password());
	}
	if (rzi_at_is_locked()) {
		if (!rzi_at_password_matches(request->argument)) {
			return -EINVAL;
		}
		rzi_at_set_locked(false);
		return rzi_at_respond_status(RZI_AT_STATUS_OK);
	}
	rc = rzi_at_set_password(request->argument);
	if (rc != 0) {
		return rc;
	}
	rc = save_string("pword", request->argument, 8U);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_baud(const struct rzi_at_request *request, void *user_data)
{
#if defined(CONFIG_RZI_AT_ADAPTER_UART)
	uint32_t baud = 0;
	char *end;
	unsigned long parsed;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = rzi_at_uart_get_baud(&baud);
		return rc != 0 ? rc : rzi_at_respond_value("AT+BAUD=%u", baud);
	}
	parsed = strtoul(request->argument, &end, 10);
	if (end == request->argument || *end != '\0' || parsed == 0UL) {
		return -EINVAL;
	}
	rc = rzi_at_uart_set_baud((uint32_t)parsed);
	return rc != 0 ? rc : rzi_at_respond_status(RZI_AT_STATUS_OK);
#else
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return -ENOTSUP;
#endif
}

static int handle_atm(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_sleep(const struct rzi_at_request *request, void *user_data)
{
#if defined(CONFIG_RZI_POWER)
	char *end;
	long parsed;

	ARG_UNUSED(user_data);
	parsed = strtol(request->argument, &end, 10);
	if (end == request->argument || *end != '\0' || parsed <= 0 || parsed > INT32_MAX) {
		return -EINVAL;
	}
	ignore_result(rzi_at_respond_status(RZI_AT_STATUS_OK));
	return rzi_power_sleep((int32_t)parsed);
#else
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return -ENOTSUP;
#endif
}

static int handle_lpm(const struct rzi_at_request *request, void *user_data)
{
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+LPM=%u", lpm_enabled);
	}
	if (strcmp(request->argument, "0") == 0) {
		lpm_enabled = 0U;
	} else if (strcmp(request->argument, "1") == 0) {
		lpm_enabled = 1U;
	} else {
		return -EINVAL;
	}
#if defined(CONFIG_RZI_POWER)
	rc = rzi_power_set_policy(lpm_enabled != 0U ? RZI_POWER_POLICY_SUSPEND
						    : RZI_POWER_POLICY_RUN);
	if (rc != 0) {
		return rc;
	}
#else
	ARG_UNUSED(rc);
#endif
	return rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_lpmlvl(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+LPMLVL=%u", lpm_level);
	}
	if (strcmp(request->argument, "1") == 0) {
		lpm_level = 1U;
	} else if (strcmp(request->argument, "2") == 0) {
		lpm_level = 2U;
	} else {
		return -EINVAL;
	}
	return rzi_at_respond_status(RZI_AT_STATUS_OK);
}

static int handle_bat(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
	return -ENOTSUP;
}

static int handle_blemac(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(request);
	ARG_UNUSED(user_data);
#if defined(CONFIG_BT)
	bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
	size_t count = ARRAY_SIZE(addrs);

	bt_id_get(addrs, &count);
	if (count == 0U) {
		return -ENOTSUP;
	}
	return rzi_at_respond_value("AT+BLEMAC=%02X:%02X:%02X:%02X:%02X:%02X", addrs[0].a.val[5],
				    addrs[0].a.val[4], addrs[0].a.val[3], addrs[0].a.val[2],
				    addrs[0].a.val[1], addrs[0].a.val[0]);
#else
	return -ENOTSUP;
#endif
}

static const struct rzi_at_command commands[] = {
	{
		.name = "VER",
		.help = "get the version of the firmware",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_version,
	},
	{
		.name = "FIRMWAREVER",
		.help = "get the custom firmware version",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_firmwarever,
	},
	{
		.name = "CLIVER",
		.help = "get the version of the AT command",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_cliver,
	},
	{
		.name = "APIVER",
		.help = "get the version of the RZI API",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_apiver,
	},
	{
		.name = "HWMODEL",
		.help = "get the string of the hardware model",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_hwmodel,
	},
	{
		.name = "HWID",
		.help = "get the string of the hardware ID",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_hwid,
	},
	{
		.name = "SN",
		.help = "get the serial number of the device (max 18 char)",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_sn,
	},
	{
		.name = "FSN",
		.help = "get the factory serial number of the device",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_sn,
	},
	{
		.name = "ALIAS",
		.help = "add an alias name to the device",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_alias,
	},
	{
		.name = "BAT",
		.help = "get the battery level (volt)",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_bat,
	},
	{
		.name = "SYSV",
		.help = "get the System Voltage",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_bat,
	},
	{
		.name = "BUILDTIME",
		.help = "get the build time of the firmware",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_buildtime,
	},
	{
		.name = "REPOINFO",
		.help = "get the commit ID of the firmware",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_repoinfo,
	},
	{
		.name = "BOOT",
		.help = "enter bootloader mode for firmware upgrade",
		.allowed_operations = RZI_AT_ALLOW_RUN,
		.handler = handle_boot,
	},
	{
		.name = "BOOTVER",
		.help = "get the version of RUI Bootloader",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_bootver,
	},
	{
		.name = "DEBUG",
		.help = "dump AT debug information",
		.allowed_operations = RZI_AT_ALLOW_RUN,
		.handler = handle_debug,
	},
	{
		.name = "FACTORY",
		.help = "restore default parameters",
		.allowed_operations = RZI_AT_ALLOW_RUN,
		.handler = handle_factory,
	},
	{
		.name = "LOCK",
		.help = "lock the serial port",
		.allowed_operations = RZI_AT_ALLOW_RUN | RZI_AT_ALLOW_READ,
		.handler = handle_lock,
	},
	{
		.name = "PWORD",
		.help = "set the serial port locking password (max 8 char)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_pword,
	},
	{
		.name = "BAUD",
		.help = "get or set the serial port baudrate",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_baud,
	},
	{
		.name = "ATM",
		.help = "switch to AT command mode",
		.allowed_operations = RZI_AT_ALLOW_RUN,
		.handler = handle_atm,
	},
	{
		.name = "SLEEP",
		.help = "enter sleep mode for a period of time (ms)",
		.allowed_operations = RZI_AT_ALLOW_WRITE,
		.handler = handle_sleep,
	},
	{
		.name = "LPM",
		.help = "get or set the low power mode (0 = off, 1 = on)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_lpm,
	},
	{
		.name = "LPMLVL",
		.help = "get or set the sleep level for low power mode",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_lpmlvl,
	},
	{
		.name = "BLEMAC",
		.help = "get the BLE MAC address",
		.allowed_operations = RZI_AT_ALLOW_READ,
		.handler = handle_blemac,
	},
};

int rzi_at_builtin_register(void)
{
	int rc = rzi_at_register(commands, ARRAY_SIZE(commands));

	if (rc == 0) {
		(void)load_string("alias", alias_name, ALIAS_MAX);
		(void)load_string("sn", serial_number, SN_MAX);
		{
			char stored[9] = {0};

			if (load_string("pword", stored, 8U) == 0 && stored[0] != '\0') {
				(void)rzi_at_set_password(stored);
			}
		}
	}
#if defined(CONFIG_RZI_AT_COMMAND_LORAWAN)
	if (rc == 0) {
		rc = rzi_at_lorawan_register();
	}
#endif
#if defined(CONFIG_RZI_AT_COMMAND_LORA)
	if (rc == 0) {
		rc = rzi_at_lora_register();
	}
#endif
	return rc;
}
