/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RUI3-compatible AT line parser and built-in command dispatcher.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

#include "at_priv.h"

static char line[CONFIG_RZI_AT_LINE_MAX];
static size_t line_length;
static bool line_overflow;
static bool ignore_lf;

static void ignore_result(int result)
{
	ARG_UNUSED(result);
}

static int ascii_casecmp_n(const char *left, const char *right, size_t count)
{
	for (size_t i = 0; i < count; ++i) {
		char a = left[i];
		char b = right[i];

		if (a >= 'a' && a <= 'z') {
			a -= 'a' - 'A';
		}
		if (b >= 'a' && b <= 'z') {
			b -= 'a' - 'A';
		}
		if (a != b || a == '\0') {
			return (unsigned char)a - (unsigned char)b;
		}
	}
	return 0;
}

static bool equals_ignore_case(const char *left, const char *right)
{
	return strlen(left) == strlen(right) && ascii_casecmp_n(left, right, strlen(left)) == 0;
}

static void respond_for_error(int rc)
{
	if (rc == -EINVAL) {
		ignore_result(rzi_at_respond_status(RZI_AT_STATUS_PARAM_ERROR));
	} else if (rc == -EBUSY) {
		ignore_result(rzi_at_respond_status(RZI_AT_STATUS_BUSY_ERROR));
	} else if (rc == -ENETDOWN) {
		ignore_result(rzi_at_respond_status(RZI_AT_STATUS_NO_NETWORK_JOINED));
	} else if (rc != 0) {
		ignore_result(rzi_at_respond_status(RZI_AT_STATUS_ERROR));
	}
}

static void uppercase_name(char *name)
{
	for (; *name != '\0'; ++name) {
		if (*name >= 'a' && *name <= 'z') {
			*name -= 'a' - 'A';
		}
	}
}

static void execute(char *input)
{
	char *body;
	char *argument = NULL;
	char *separator;
	enum rzi_at_operation operation;
	int rc;

	if (equals_ignore_case(input, "AT")) {
		ignore_result(rzi_at_respond_status(RZI_AT_STATUS_OK));
		return;
	}
	if (equals_ignore_case(input, "ATZ")) {
		sys_reboot(SYS_REBOOT_COLD);
		return;
	}
	if (equals_ignore_case(input, "ATZ?")) {
		ignore_result(rzi_at_respond_value("ATZ: triggers a reset on the MCU."));
		return;
	}
	if (equals_ignore_case(input, "ATR")) {
		rc = rzi_at_registry_factory_reset();
		ignore_result(
			rzi_at_respond_status(rc == 0 ? RZI_AT_STATUS_OK : RZI_AT_STATUS_ERROR));
		if (rc == 0) {
			sys_reboot(SYS_REBOOT_COLD);
		}
		return;
	}
	if (equals_ignore_case(input, "ATR?")) {
		ignore_result(rzi_at_respond_value("ATR: restore default parameters"));
		return;
	}
	if (ascii_casecmp_n(input, "AT+", 3) != 0) {
		ignore_result(rzi_at_respond_status(RZI_AT_STATUS_ERROR));
		return;
	}

	body = input + 3;
	separator = strpbrk(body, "=?");
	if (separator == NULL) {
		operation = RZI_AT_OP_RUN;
	} else if (strcmp(separator, "?") == 0) {
		operation = RZI_AT_OP_HELP;
		*separator = '\0';
	} else if (strcmp(separator, "=?") == 0) {
		operation = RZI_AT_OP_READ;
		*separator = '\0';
	} else if (*separator == '=' && separator[1] != '\0' &&
		   strchr(separator + 1, '?') == NULL) {
		operation = RZI_AT_OP_WRITE;
		*separator = '\0';
		argument = separator + 1;
	} else {
		ignore_result(rzi_at_respond_status(RZI_AT_STATUS_ERROR));
		return;
	}
	if (body[0] == '\0') {
		ignore_result(rzi_at_respond_status(RZI_AT_STATUS_ERROR));
		return;
	}
	uppercase_name(body);
	respond_for_error(rzi_at_dispatch(body, operation, argument));
}

int rzi_at_parser_feed(uint8_t byte)
{
	if (byte == '\n' && ignore_lf) {
		ignore_lf = false;
		return 0;
	}
	ignore_lf = false;
	if (byte == '\b' || byte == 0x7fU) {
		if (line_length != 0U) {
			--line_length;
#if defined(CONFIG_RZI_AT_ECHO)
			(void)rzi_at_write_raw("\b \b");
#endif
		}
		return 0;
	}
	if (byte == '\r' || byte == '\n') {
		ignore_lf = byte == '\r';
#if defined(CONFIG_RZI_AT_ECHO)
		if (byte == '\r') {
			(void)rzi_at_write_raw("\r\n");
		}
#endif
		if (line_overflow) {
			line_overflow = false;
			line_length = 0U;
			return rzi_at_respond_status(RZI_AT_STATUS_TEST_PARAM_OVERFLOW);
		}
		if (line_length != 0U) {
			line[line_length] = '\0';
			line_length = 0U;
			execute(line);
		}
		return 0;
	}
	if (byte < ' ' || byte > '~') {
		return 0;
	}
	if (line_length >= sizeof(line) - 1U) {
		line_overflow = true;
		return -ENOSPC;
	}
	line[line_length++] = (char)byte;
#if defined(CONFIG_RZI_AT_ECHO)
	char echo[2] = {(char)byte, '\0'};

	(void)rzi_at_write_raw(echo);
#endif
	return 0;
}

void rzi_at_parser_reset(void)
{
	line_length = 0U;
	line_overflow = false;
	ignore_lf = false;
}
