/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <rzi/at/at.h>

K_SEM_DEFINE(response_ready, 0, 16);
K_MUTEX_DEFINE(output_lock);

static char output[2048];
static size_t output_size;
static char last_argument[32];

static int io_write(const uint8_t *data, size_t size, void *user_data)
{
	ARG_UNUSED(user_data);

	k_mutex_lock(&output_lock, K_FOREVER);
	if (output_size + size >= sizeof(output)) {
		k_mutex_unlock(&output_lock);
		return -RZI_ERR_OVERFLOW;
	}
	memcpy(output + output_size, data, size);
	output_size += size;
	output[output_size] = '\0';
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
		if (strcmp(request->argument, "LARGE") == 0) {
			return -RZI_ERR_TOO_LARGE;
		}
		if (strcmp(request->argument, "OVER") == 0) {
			return -RZI_ERR_OVERFLOW;
		}
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

static void reset_output(void)
{
	k_mutex_lock(&output_lock, K_FOREVER);
	output_size = 0;
	output[0] = '\0';
	k_mutex_unlock(&output_lock);
	k_sem_reset(&response_ready);
}

static void send_command(const char *command)
{
	reset_output();
	zassert_ok(rzi_at_receive((const uint8_t *)command, strlen(command)));
	zassert_ok(k_sem_take(&response_ready, K_SECONDS(1)));
}

static void send_command_until(const char *command, const char *expected)
{
	int64_t deadline = k_uptime_get() + 1000;

	reset_output();
	zassert_ok(rzi_at_receive((const uint8_t *)command, strlen(command)));
	for (;;) {
		bool found;
		int64_t remaining = deadline - k_uptime_get();

		zassert_true(remaining > 0);
		zassert_ok(k_sem_take(&response_ready, K_MSEC(remaining)));
		k_mutex_lock(&output_lock, K_FOREVER);
		found = strstr(output, expected) != NULL;
		k_mutex_unlock(&output_lock);
		if (found) {
			return;
		}
	}
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
	zassert_not_null(strstr(output, "\r\nOK\r\n"));
}

ZTEST(rzi_at_core, test_size_errors_map_to_rui3_overflow)
{
	send_command_until("AT+CUSTOM=LARGE\r", "\r\nAT_TEST_PARAM_OVERFLOW\r\n");
	send_command_until("AT+CUSTOM=OVER\r", "\r\nAT_TEST_PARAM_OVERFLOW\r\n");
}

ZTEST(rzi_at_core, test_operation_validation)
{
	send_command("AT+CUSTOM\r");
	zassert_not_null(strstr(output, "\r\nAT_ERROR\r\n"));
}

ZTEST(rzi_at_core, test_registry_is_immutable_after_start)
{
	zassert_equal(rzi_at_register(&custom_command, 1), -RZI_ERR_DENIED);
}

ZTEST(rzi_at_core, test_system_lock_and_sleep)
{
	send_command_until("AT+PWORD=secret123\r", "AT_PARAM_ERROR");
	send_command_until("AT+PWORD=secret\r", "\r\nOK\r\n");
	send_command_until("AT+LOCK\r", "\r\nOK\r\n");
	send_command_until("AT+VER=?\r", "AT_ERROR");
	send_command_until("AT+PWORD=secret\r", "\r\nOK\r\n");
	send_command_until("AT+VER=?\r", "AT+VER=RZI_");
	send_command_until("AT+SLEEP=10\r", "AT_ERROR");
	send_command_until("AT+BLEMAC=?\r", "AT_ERROR");
}

#if defined(CONFIG_RZI_AT_COMMAND_LORA)
ZTEST(rzi_at_core, test_p2p_and_radio_test_commands)
{
	send_command_until("AT+P2P=868000000:7:0:0:8:14\r", "\r\nOK\r\n");
	send_command_until("AT+PFREQ=?\r", "AT+PFREQ=868000000");
	send_command_until("AT+PSEND=AABB\r", "TXP2P DONE");
	send_command_until("AT+PRECV=1000\r", "RXP2P:");
	send_command_until("AT+TRSSI\r", "\r\nOK\r\n");
	send_command_until("AT+TTX=1\r", "\r\nOK\r\n");
	send_command_until("AT+TOFF\r", "\r\nOK\r\n");
	send_command_until("AT+CW=868000000:14:100\r", "\r\nOK\r\n");
}
#endif

ZTEST_SUITE(rzi_at_core, NULL, setup, NULL, NULL, NULL);
