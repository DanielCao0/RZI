/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Raw LoRa and FSK service implementation.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <rzi/lora/lora.h>

#include "backend/lora_backend.h"

static K_MUTEX_DEFINE(lock);
static atomic_t started;
static enum rzi_lora_modulation modulation = RZI_LORA_MOD_LORA;
static struct rzi_lora_config config = {
	.frequency_hz = 868000000U,
	.spreading_factor = 7U,
	.bandwidth = 0U,
	.coding_rate = 0U,
	.preamble_length = 8U,
	.tx_power_dbm = 14,
	.sync_word = 0x3444U,
	.fsk_bitrate = 49152U,
	.fsk_deviation = 5000U,
};
static struct rzi_lora_callbacks callbacks;
static atomic_t rx_active;

static int check_thread(void)
{
	return k_is_in_isr() ? -RZI_ERR_WOULDBLOCK : 0;
}

static void apply_cipher(uint8_t *data, size_t size, const struct rzi_lora_config *settings)
{
	if (!settings->encrypt) {
		return;
	}
	for (size_t i = 0; i < size; ++i) {
		data[i] ^= settings->key[i % 16U] ^ settings->iv[i % 16U];
	}
}

void rzi_lora_publish_tx_done(int error)
{
	if (callbacks.tx_done != NULL) {
		callbacks.tx_done(error, callbacks.user_data);
	}
}

void rzi_lora_publish_rx(const uint8_t *data, size_t size, int16_t rssi_dbm, int8_t snr_quarter_db)
{
	uint8_t payload[RZI_LORA_MAX_PAYLOAD];
	size_t copy = size > sizeof(payload) ? sizeof(payload) : size;

	if (data == NULL || copy == 0U) {
		return;
	}
	memcpy(payload, data, copy);
	apply_cipher(payload, copy, &config);
	if (callbacks.rx_done != NULL) {
		callbacks.rx_done(payload, copy, rssi_dbm, snr_quarter_db, callbacks.user_data);
	}
}

int rzi_lora_register_callbacks(const struct rzi_lora_callbacks *table)
{
	int rc = check_thread();

	if (rc != 0) {
		return rc;
	}
	if (table == NULL) {
		return -RZI_ERR_INVALID;
	}
	k_mutex_lock(&lock, K_FOREVER);
	callbacks = *table;
	k_mutex_unlock(&lock);
	return 0;
}

int rzi_lora_start(enum rzi_lora_modulation value)
{
	int rc = check_thread();

	if (rc != 0) {
		return rc;
	}
	if (value != RZI_LORA_MOD_LORA && value != RZI_LORA_MOD_FSK) {
		return -RZI_ERR_INVALID;
	}
	if (rzi_lora_backend.start == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	k_mutex_lock(&lock, K_FOREVER);
	rc = rzi_lora_backend.start(value);
	if (rc == 0) {
		modulation = value;
		atomic_set(&started, 1);
		if (rzi_lora_backend.apply_config != NULL) {
			rc = rzi_lora_backend.apply_config(&config);
		}
	}
	k_mutex_unlock(&lock);
	return rc;
}

int rzi_lora_stop(void)
{
	int rc = check_thread();

	if (rc != 0) {
		return rc;
	}
	k_mutex_lock(&lock, K_FOREVER);
	if (rzi_lora_backend.stop != NULL) {
		rc = rzi_lora_backend.stop();
	}
	if (rc == 0) {
		atomic_clear(&started);
		atomic_clear(&rx_active);
	}
	k_mutex_unlock(&lock);
	return rc;
}

bool rzi_lora_is_started(void)
{
	return atomic_get(&started) != 0;
}

int rzi_lora_get_config(struct rzi_lora_config *out)
{
	int rc = check_thread();

	if (rc != 0) {
		return rc;
	}
	if (out == NULL) {
		return -RZI_ERR_INVALID;
	}
	k_mutex_lock(&lock, K_FOREVER);
	*out = config;
	k_mutex_unlock(&lock);
	return 0;
}

int rzi_lora_set_config(const struct rzi_lora_config *in)
{
	int rc = check_thread();

	if (rc != 0) {
		return rc;
	}
	if (in == NULL || in->spreading_factor < 5U || in->spreading_factor > 12U ||
	    in->tx_power_dbm < 5 || in->tx_power_dbm > 22 || in->preamble_length < 5U ||
	    in->coding_rate > 3U) {
		return -RZI_ERR_INVALID;
	}
	k_mutex_lock(&lock, K_FOREVER);
	config = *in;
	if (atomic_get(&started) && rzi_lora_backend.apply_config != NULL) {
		rc = rzi_lora_backend.apply_config(&config);
	}
	k_mutex_unlock(&lock);
	return rc;
}

int rzi_lora_send(const uint8_t *data, size_t size)
{
	uint8_t framed[RZI_LORA_MAX_PAYLOAD];
	int rc = check_thread();

	if (rc != 0) {
		return rc;
	}
	if (data == NULL || size == 0U || size > RZI_LORA_MAX_PAYLOAD) {
		return -RZI_ERR_INVALID;
	}
	if (!atomic_get(&started) || rzi_lora_backend.send == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	memcpy(framed, data, size);
	k_mutex_lock(&lock, K_FOREVER);
	apply_cipher(framed, size, &config);
	k_mutex_unlock(&lock);
	return rzi_lora_backend.send(framed, size);
}

int rzi_lora_receive(uint32_t timeout_ms)
{
	int rc = check_thread();

	if (rc != 0) {
		return rc;
	}
	if (!atomic_get(&started) || rzi_lora_backend.receive == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	rc = rzi_lora_backend.receive(timeout_ms);
	if (rc == 0) {
		if (timeout_ms == 0U) {
			atomic_clear(&rx_active);
		} else {
			atomic_set(&rx_active, 1);
		}
	}
	return rc;
}

bool rzi_lora_receive_active(void)
{
	return atomic_get(&rx_active) != 0;
}

void rzi_lora_poll(void)
{
	if (rzi_lora_backend.poll != NULL) {
		rzi_lora_backend.poll();
	}
}

static int call_test(int (*fn)(void))
{
	int rc = check_thread();

	if (rc != 0) {
		return rc;
	}
	if (fn == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return fn();
}

int rzi_lora_test_rssi(void)
{
	return call_test(rzi_lora_backend.test_rssi);
}

int rzi_lora_test_tone(void)
{
	return call_test(rzi_lora_backend.test_tone);
}

int rzi_lora_test_tx(uint32_t packet_count)
{
	int rc = check_thread();

	if (rc != 0) {
		return rc;
	}
	if (rzi_lora_backend.test_tx == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return rzi_lora_backend.test_tx(packet_count);
}

int rzi_lora_test_rx(uint32_t packet_count)
{
	int rc = check_thread();

	if (rc != 0) {
		return rc;
	}
	if (rzi_lora_backend.test_rx == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return rzi_lora_backend.test_rx(packet_count);
}

int rzi_lora_test_cw(uint32_t frequency_hz, int8_t power_dbm, uint32_t duration_ms)
{
	int rc = check_thread();

	if (rc != 0) {
		return rc;
	}
	if (rzi_lora_backend.test_cw == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return rzi_lora_backend.test_cw(frequency_hz, power_dbm, duration_ms);
}

int rzi_lora_test_stop(void)
{
	return call_test(rzi_lora_backend.test_stop);
}
