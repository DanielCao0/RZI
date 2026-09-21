/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Optional Zephyr LoRaWAN feature operations for the RZI backend.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/lorawan/lorawan.h>
#include <zephyr/sys/util.h>

#include "lorawan_backend_zephyr_priv.h"

static enum rzi_lorawan_class device_class = RZI_LORAWAN_CLASS_A;
static bool adr_enabled = true;
static enum rzi_lorawan_data_rate data_rate = RZI_LORAWAN_DR_0;
static uint16_t channel_mask[RZI_LORAWAN_CHANNEL_MASK_WORDS];
static bool channel_mask_valid;

static size_t mask_words_for_region(enum rzi_lorawan_region region)
{
	switch (region) {
	case RZI_LORAWAN_REGION_US_915:
	case RZI_LORAWAN_REGION_AU_915:
	case RZI_LORAWAN_REGION_CN_470:
		return RZI_LORAWAN_CHANNEL_MASK_WORDS;
	default:
		return 1U;
	}
}

int zephyr_get_class(enum rzi_lorawan_class *value)
{
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	*value = device_class;
	rzi_lorawan_zephyr_unlock();
	return 0;
}

static int zephyr_get_adr(bool *enabled)
{
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	*enabled = adr_enabled;
	rzi_lorawan_zephyr_unlock();
	return 0;
}

static int zephyr_set_adr(bool enabled)
{
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	lorawan_enable_adr(enabled);
	adr_enabled = enabled;
	rzi_lorawan_zephyr_unlock();
	return 0;
}

static int zephyr_get_data_rate(enum rzi_lorawan_data_rate *value)
{
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	*value = data_rate;
	rzi_lorawan_zephyr_unlock();
	return 0;
}

static int zephyr_set_data_rate(enum rzi_lorawan_data_rate value)
{
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	rc = lorawan_set_datarate((enum lorawan_datarate)value);
	if (rc == 0) {
		data_rate = value;
	}
	rzi_lorawan_zephyr_unlock();
	return rzi_err_from_errno(rc);
}

static int zephyr_get_channel_mask(uint16_t *mask, size_t words)
{
	size_t copy;
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	if (!channel_mask_valid) {
		rzi_lorawan_zephyr_unlock();
		return -RZI_ERR_NO_DATA;
	}
	copy = MIN(words, ARRAY_SIZE(channel_mask));
	memset(mask, 0, words * sizeof(uint16_t));
	memcpy(mask, channel_mask, copy * sizeof(uint16_t));
	rzi_lorawan_zephyr_unlock();
	return 0;
}

static int zephyr_set_channel_mask(const uint16_t *mask, size_t words)
{
	size_t required;
	uint16_t local[RZI_LORAWAN_CHANNEL_MASK_WORDS] = {0};
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	required = mask_words_for_region(rzi_lorawan_zephyr_region());
	if (words < required) {
		rzi_lorawan_zephyr_unlock();
		return -RZI_ERR_INVALID;
	}
	memcpy(local, mask, required * sizeof(uint16_t));
#ifndef CONFIG_LORAWAN_EMUL
	rc = lorawan_set_channels_mask(local, required);
	if (rc == 0) {
#endif
		memset(channel_mask, 0, sizeof(channel_mask));
		memcpy(channel_mask, local, required * sizeof(uint16_t));
		channel_mask_valid = true;
#ifndef CONFIG_LORAWAN_EMUL
	}
#endif
	rzi_lorawan_zephyr_unlock();
	return rzi_err_from_errno(rc);
}

const struct rzi_lorawan_network_ops zephyr_network_ops = {
	.get_adr = zephyr_get_adr,
	.set_adr = zephyr_set_adr,
	.get_data_rate = zephyr_get_data_rate,
	.set_data_rate = zephyr_set_data_rate,
};

const struct rzi_lorawan_channel_ops zephyr_channel_ops = {
	.get_channel_mask = zephyr_get_channel_mask,
	.set_channel_mask = zephyr_set_channel_mask,
};

int zephyr_query_tx_possible(size_t size)
{
	uint8_t next_size = 0;
	uint8_t max_size = 0;
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	lorawan_get_payload_sizes(&next_size, &max_size);
	rzi_lorawan_zephyr_unlock();
	return size > next_size ? -RZI_ERR_TOO_LARGE : 0;
}

static int zephyr_get_dev_nonce(uint16_t *dev_nonce)
{
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	*dev_nonce = rzi_lorawan_zephyr_dev_nonce();
	rzi_lorawan_zephyr_unlock();
	return 0;
}

static int zephyr_set_dev_nonce(uint16_t dev_nonce)
{
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	rzi_lorawan_zephyr_set_dev_nonce(dev_nonce);
	rzi_lorawan_zephyr_unlock();
	return 0;
}

int zephyr_is_busy(bool *busy)
{
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	*busy = rzi_lorawan_zephyr_busy();
	rzi_lorawan_zephyr_unlock();
	return 0;
}

const struct rzi_lorawan_session_ops zephyr_session_ops = {
	.get_dev_nonce = zephyr_get_dev_nonce,
	.set_dev_nonce = zephyr_set_dev_nonce,
};

static int zephyr_link_check_request(void)
{
#ifdef CONFIG_LORAWAN_EMUL
	return -RZI_ERR_NOT_SUPPORTED;
#else
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	rzi_lorawan_zephyr_unlock();
	return rzi_err_from_errno(lorawan_request_link_check(true));
#endif
}

static int zephyr_device_time_request(void)
{
#ifdef CONFIG_LORAWAN_EMUL
	return -RZI_ERR_NOT_SUPPORTED;
#else
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	rzi_lorawan_zephyr_unlock();
	return rzi_err_from_errno(lorawan_request_device_time(true));
#endif
}

static int zephyr_get_network_time(struct rzi_lorawan_network_time *time)
{
#ifdef CONFIG_LORAWAN_EMUL
	ARG_UNUSED(time);
	return -RZI_ERR_NOT_SUPPORTED;
#else
	uint32_t gps_time = 0;
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	rzi_lorawan_zephyr_unlock();
	rc = lorawan_device_time_get(&gps_time);
	if (rc != 0) {
		return rzi_err_from_errno(rc);
	}
	time->gps_seconds = gps_time;
	time->gps_subseconds = 0;
	return 0;
#endif
}

const struct rzi_lorawan_mac_ops zephyr_mac_ops = {
	.request_link_check = zephyr_link_check_request,
	.request_device_time = zephyr_device_time_request,
	.get_network_time = zephyr_get_network_time,
};

void rzi_lorawan_zephyr_set_class(enum rzi_lorawan_class value)
{
	device_class = value;
}
