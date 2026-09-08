/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <rzi/at.h>

K_SEM_DEFINE(response_ready, 0, 1);
K_MUTEX_DEFINE(output_lock);

static char output[256];
static char last_argument[32];

static int io_write(const uint8_t *data, size_t size, void *user_data)
{
	ARG_UNUSED(user_data);
	if (size >= sizeof(output)) {
		return -ENOSPC;
	}
	k_mutex_lock(&output_lock, K_FOREVER);
	memcpy(output, data, size);
	output[size] = '\0';
	k_mutex_unlock(&output_lock);
	k_sem_give(&response_ready);
	return 0;
}

static int custom_handler(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_WRITE) {
		strncpy(last_argument, request->argument, sizeof(last_argument) - 1U);
		last_argument[sizeof(last_argument) - 1U] = '\0';
		return rzi_at_respond_status(RZI_AT_STATUS_OK);
	}
	return rzi_at_respond_value("AT+CUSTOM=value");
}

static const struct rzi_at_command custom_command = {
	.name = "CUSTOM",
	.help = "test command",
	.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
	.handler = custom_handler,
};

static void send_command(const char *command)
{
	k_mutex_lock(&output_lock, K_FOREVER);
	output[0] = '\0';
	k_mutex_unlock(&output_lock);
	k_sem_reset(&response_ready);
	zassert_ok(rzi_at_receive((const uint8_t *)command, strlen(command)));
	zassert_ok(k_sem_take(&response_ready, K_SECONDS(1)));
}

static void *setup(void)
{
	static const struct rzi_at_io io = {
		.write = io_write,
	};

	zassert_ok(rzi_at_register(&custom_command, 1));
	zassert_ok(rzi_at_start(&io));
	return NULL;
}

ZTEST(rzi_at_core, test_attention_and_crlf)
{
	send_command("AT\r\n");
	zassert_equal(strcmp(output, "\r\nOK\r\n"), 0);
}

ZTEST(rzi_at_core, test_command_name_is_case_insensitive)
{
	send_command("at+ver=?\r");
	zassert_not_null(strstr(output, "AT+VER=RZI_0.2.0_"));
}

ZTEST(rzi_at_core, test_argument_case_is_preserved)
{
	send_command("AT+CUSTOM=AbCd12\r");
	zassert_equal(strcmp(last_argument, "AbCd12"), 0);
	zassert_equal(strcmp(output, "\r\nOK\r\n"), 0);
}

ZTEST(rzi_at_core, test_operation_validation)
{
	send_command("AT+CUSTOM\r");
	zassert_equal(strcmp(output, "\r\nAT_ERROR\r\n"), 0);
}

ZTEST(rzi_at_core, test_registry_is_immutable_after_start)
{
	zassert_equal(rzi_at_register(&custom_command, 1), -EACCES);
}

ZTEST_SUITE(rzi_at_core, NULL, setup, NULL, NULL, NULL);
