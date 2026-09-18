/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Software raw LoRa backend for host tests.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>

#include "lora_backend.h"

static uint8_t last_payload[RZI_LORA_MAX_PAYLOAD];
static size_t last_size;

static int test_start(enum rzi_lora_modulation modulation)
{
	ARG_UNUSED(modulation);
	last_size = 0U;
	return 0;
}

static int test_stop(void)
{
	last_size = 0U;
	return 0;
}

static int test_apply(const struct rzi_lora_config *config)
{
	ARG_UNUSED(config);
	return 0;
}

static int test_send(const uint8_t *data, size_t size)
{
	memcpy(last_payload, data, size);
	last_size = size;
	rzi_lora_publish_tx_done(0);
	return 0;
}

static int test_receive(uint32_t timeout_ms)
{
	if (timeout_ms == 0U) {
		return 0;
	}
	if (last_size > 0U) {
		rzi_lora_publish_rx(last_payload, last_size, -80, 0);
	}
	return 0;
}

static int test_ok(void)
{
	return 0;
}

static int test_count(uint32_t packet_count)
{
	ARG_UNUSED(packet_count);
	return 0;
}

static int test_cw(uint32_t frequency_hz, int8_t power_dbm, uint32_t duration_ms)
{
	ARG_UNUSED(frequency_hz);
	ARG_UNUSED(power_dbm);
	ARG_UNUSED(duration_ms);
	return 0;
}

const struct rzi_lora_backend_api rzi_lora_backend = {
	.start = test_start,
	.stop = test_stop,
	.apply_config = test_apply,
	.send = test_send,
	.receive = test_receive,
	.test_rssi = test_ok,
	.test_tone = test_ok,
	.test_tx = test_count,
	.test_rx = test_count,
	.test_cw = test_cw,
	.test_stop = test_ok,
};
