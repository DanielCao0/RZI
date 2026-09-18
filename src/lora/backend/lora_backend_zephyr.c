/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Zephyr LoRa-driver backend for raw LoRa P2P.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>

#include "lora_backend.h"

#define LORA_NODE DT_ALIAS(lora0)

static enum rzi_lora_modulation modulation = RZI_LORA_MOD_LORA;

static const struct device *radio_device(void)
{
#if DT_NODE_EXISTS(LORA_NODE)
	return DEVICE_DT_GET(LORA_NODE);
#else
	return NULL;
#endif
}

static enum lora_signal_bandwidth map_bandwidth(uint32_t index)
{
	static const enum lora_signal_bandwidth table[] = {
		BW_125_KHZ, BW_250_KHZ, BW_500_KHZ, BW_7_KHZ,  BW_10_KHZ,
		BW_15_KHZ,  BW_20_KHZ,  BW_31_KHZ,  BW_41_KHZ, BW_62_KHZ,
	};

	if (index >= ARRAY_SIZE(table)) {
		return BW_125_KHZ;
	}
	return table[index];
}

static int fill_config(const struct rzi_lora_config *config, bool tx, struct lora_modem_config *out)
{
	if (modulation != RZI_LORA_MOD_LORA) {
		return -ENOTSUP;
	}
	memset(out, 0, sizeof(*out));
	out->frequency = config->frequency_hz;
	out->bandwidth = map_bandwidth(config->bandwidth);
	out->datarate = (enum lora_datarate)config->spreading_factor;
	out->coding_rate = (enum lora_coding_rate)(config->coding_rate + 1U);
	out->preamble_len = config->preamble_length;
	out->tx_power = config->tx_power_dbm;
	out->tx = tx;
	out->iq_inverted = config->iq_inverted;
	out->public_network = false;
	if ((config->sync_word & 0xffU) != 0U) {
		out->sync_word = (uint8_t)(config->sync_word & 0xffU);
	}
	return 0;
}

static int zephyr_start(enum rzi_lora_modulation value)
{
	const struct device *dev = radio_device();

	if (dev == NULL || !device_is_ready(dev)) {
		return -ENODEV;
	}
	modulation = value;
	return 0;
}

static int zephyr_stop(void)
{
	return 0;
}

static int zephyr_apply(const struct rzi_lora_config *config)
{
	ARG_UNUSED(config);
	return 0;
}

static int zephyr_send(const uint8_t *data, size_t size)
{
	const struct device *dev = radio_device();
	struct lora_modem_config modem;
	struct rzi_lora_config config;
	int rc;

	if (dev == NULL) {
		return -ENODEV;
	}
	rc = rzi_lora_get_config(&config);
	if (rc != 0) {
		return rc;
	}
	rc = fill_config(&config, true, &modem);
	if (rc != 0) {
		return rc;
	}
	rc = lora_config(dev, &modem);
	if (rc != 0) {
		return rc;
	}
	rc = lora_send(dev, (uint8_t *)data, size);
	rzi_lora_publish_tx_done(rc);
	return rc;
}

static int zephyr_receive(uint32_t timeout_ms)
{
	const struct device *dev = radio_device();
	struct lora_modem_config modem;
	struct rzi_lora_config config;
	uint8_t payload[RZI_LORA_MAX_PAYLOAD];
	int16_t rssi = 0;
	int8_t snr = 0;
	int rc;

	if (timeout_ms == 0U) {
		return 0;
	}
	if (dev == NULL) {
		return -ENODEV;
	}
	rc = rzi_lora_get_config(&config);
	if (rc != 0) {
		return rc;
	}
	rc = fill_config(&config, false, &modem);
	if (rc != 0) {
		return rc;
	}
	rc = lora_config(dev, &modem);
	if (rc != 0) {
		return rc;
	}
	rc = lora_recv(dev, payload, sizeof(payload),
		       timeout_ms == 65535U ? K_FOREVER : K_MSEC(timeout_ms), &rssi, &snr);
	if (rc > 0) {
		rzi_lora_publish_rx(payload, (size_t)rc, rssi, (int8_t)(snr * 4));
		return 0;
	}
	return rc;
}

static int unsupported(void)
{
	return -ENOTSUP;
}

static int unsupported_count(uint32_t packet_count)
{
	ARG_UNUSED(packet_count);
	return -ENOTSUP;
}

static int unsupported_cw(uint32_t frequency_hz, int8_t power_dbm, uint32_t duration_ms)
{
	ARG_UNUSED(frequency_hz);
	ARG_UNUSED(power_dbm);
	ARG_UNUSED(duration_ms);
	return -ENOTSUP;
}

const struct rzi_lora_backend_api rzi_lora_backend = {
	.start = zephyr_start,
	.stop = zephyr_stop,
	.apply_config = zephyr_apply,
	.send = zephyr_send,
	.receive = zephyr_receive,
	.test_rssi = unsupported,
	.test_tone = unsupported,
	.test_tx = unsupported_count,
	.test_rx = unsupported_count,
	.test_cw = unsupported_cw,
	.test_stop = unsupported,
};
