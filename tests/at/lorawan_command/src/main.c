/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/ztest.h>

#include <rzi/at.h>
#include <rzi/lorawan.h>

#include "lorawan_backend.h"

K_SEM_DEFINE(output_ready, 0, 16);
K_MUTEX_DEFINE(output_lock);

static char output[2048];
static size_t output_size;
static rzi_lorawan_event_sink_t fake_sink;
static atomic_t fake_joined;
static struct rzi_lorawan_join_config received_join_config;
static uint8_t received_port;
static uint8_t received_payload[8];
static size_t received_size;
static enum rzi_lorawan_message_type received_message_type;

static int io_write(const uint8_t *data, size_t size, void *user_data)
{
	ARG_UNUSED(user_data);

	k_mutex_lock(&output_lock, K_FOREVER);
	if (output_size + size >= sizeof(output)) {
		k_mutex_unlock(&output_lock);
		return -ENOSPC;
	}
	memcpy(output + output_size, data, size);
	output_size += size;
	output[output_size] = '\0';
	k_mutex_unlock(&output_lock);
	k_sem_give(&output_ready);
	return 0;
}

static int fake_start(enum rzi_lorawan_region region, bool join_backoff_bypass,
		      rzi_lorawan_event_sink_t event_sink)
{
	const struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_READY,
	};

	zassert_equal(region, RZI_LORAWAN_REGION_EU_868);
	zassert_false(join_backoff_bypass);
	fake_sink = event_sink;
	fake_sink(&event);
	return 0;
}

static int fake_join(const struct rzi_lorawan_join_config *config)
{
	const struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_JOINED,
	};

	received_join_config = *config;
	atomic_set(&fake_joined, 1);
	fake_sink(&event);
	return 0;
}

static int fake_leave(void)
{
	atomic_clear(&fake_joined);
	return 0;
}

static int fake_send(uint8_t port, const uint8_t *data, size_t size,
		     enum rzi_lorawan_message_type type)
{
	const struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_TX_DONE,
		.tx_status = RZI_LORAWAN_TX_ACKED,
	};

	received_port = port;
	received_size = size;
	received_message_type = type;
	memcpy(received_payload, data, size);
	fake_sink(&event);
	return 0;
}

static int fake_set_class(enum rzi_lorawan_class device_class)
{
	return device_class == RZI_LORAWAN_CLASS_A ? 0 : -ENOTSUP;
}

static int fake_is_joined(bool *joined)
{
	*joined = atomic_get(&fake_joined) != 0;
	return 0;
}

const struct rzi_lorawan_backend_api rzi_lorawan_backend = {
	.capabilities = RZI_LORAWAN_CAP_OTAA | RZI_LORAWAN_CAP_CLASS_A,
	.start = fake_start,
	.join = fake_join,
	.leave = fake_leave,
	.send = fake_send,
	.set_class = fake_set_class,
	.is_joined = fake_is_joined,
};

static void reset_output(void)
{
	k_mutex_lock(&output_lock, K_FOREVER);
	output_size = 0;
	output[0] = '\0';
	k_mutex_unlock(&output_lock);
	k_sem_reset(&output_ready);
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
		zassert_ok(k_sem_take(&output_ready, K_MSEC(remaining)));
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

	zassert_ok(rzi_at_start(&io));
	return NULL;
}

ZTEST(rzi_at_lorawan_command, test_rui3_compatible_otaa_workflow)
{
	static const uint8_t expected_dev_eui[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
	static const uint8_t expected_join_eui[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	static const uint8_t expected_app_key[] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
	};

	send_command_until("AT+NWM=?\r", "AT+NWM=1");
	send_command_until("AT+NWM=0\r", "AT_PARAM_ERROR");
	send_command_until("AT+DEVEUI?\r", "AT+DEVEUI: get or set the device EUI (8 bytes in hex)");
	send_command_until("AT+DEVEUI\r", "AT_ERROR");
	send_command_until("AT+NJS\r", "AT_ERROR");
	send_command_until("AT+SEND=?\r", "AT_ERROR");
	send_command_until("AT+DEVEUI=1122334455667788\r", "\r\nOK\r\n");
	send_command_until("AT+DEVEUI=11223344556677GG\r", "AT_PARAM_ERROR");
	send_command_until("AT+DEVEUI=?\r", "AT+DEVEUI=1122334455667788");
	send_command_until("AT+APPEUI=0102030405060708\r", "\r\nOK\r\n");
	send_command_until("AT+APPKEY=00112233445566778899AABBCCDDEEFF\r", "\r\nOK\r\n");
	send_command_until("AT+JOIN=1:0:6:1\r", "AT_PARAM_ERROR");
	send_command_until("AT+JOIN=0:0:7:2\r", "\r\nOK\r\n");
	send_command_until("AT+JOIN=?\r", "AT+JOIN=1:0:7:2");
	send_command_until("AT+JOIN\r", "+EVT:JOINED");

	zassert_equal(received_join_config.activation, RZI_LORAWAN_ACTIVATION_OTAA);
	zassert_mem_equal(received_join_config.otaa.dev_eui, expected_dev_eui,
			  sizeof(expected_dev_eui));
	zassert_mem_equal(received_join_config.otaa.join_eui, expected_join_eui,
			  sizeof(expected_join_eui));
	zassert_mem_equal(received_join_config.otaa.application_key, expected_app_key,
			  sizeof(expected_app_key));
	zassert_mem_equal(received_join_config.otaa.network_key, expected_app_key,
			  sizeof(expected_app_key));

	send_command_until("AT+NJS=?\r", "AT+NJS=1");
	send_command_until("AT+CFM=1\r", "\r\nOK\r\n");
	send_command_until("AT+SEND=12:1234\r", "+EVT:SEND_CONFIRMED_OK");
	zassert_equal(received_port, 12);
	zassert_equal(received_size, 2);
	zassert_equal(received_message_type, RZI_LORAWAN_MSG_CONFIRMED);
	zassert_equal(received_payload[0], 0x12);
	zassert_equal(received_payload[1], 0x34);

	{
		const struct rzi_lorawan_backend_event downlink = {
			.type = RZI_LORAWAN_BACKEND_DOWNLINK,
			.downlink.port = 3,
			.downlink.size = 2,
			.downlink.rssi_dbm = -70,
			.downlink.snr_quarter_db = 32,
			.downlink.data = {0xab, 0xcd},
		};

		reset_output();
		fake_sink(&downlink);
		zassert_ok(k_sem_take(&output_ready, K_SECONDS(1)));
		k_mutex_lock(&output_lock, K_FOREVER);
		zassert_not_null(strstr(output, "+EVT:RX_1:-70:8:UNICAST:3:ABCD"));
		k_mutex_unlock(&output_lock);
	}
	send_command_until("AT+RECV=?\r", "AT+RECV=3:ABCD");
	send_command_until("AT+RECV=?\r", "AT+RECV=");
}

ZTEST_SUITE(rzi_at_lorawan_command, NULL, setup, NULL, NULL, NULL);
