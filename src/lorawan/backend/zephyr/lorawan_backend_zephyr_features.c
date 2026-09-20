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

#include "../../mac_commands/lorawan_mac_commands.h"
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

static int zephyr_get_class(enum rzi_lorawan_class *value)
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

static const struct rzi_lorawan_network_ops zephyr_network_ops = {
	.get_class = zephyr_get_class,
	.get_adr = zephyr_get_adr,
	.set_adr = zephyr_set_adr,
	.get_data_rate = zephyr_get_data_rate,
	.set_data_rate = zephyr_set_data_rate,
};

static const struct rzi_lorawan_channel_ops zephyr_channel_ops = {
	.get_channel_mask = zephyr_get_channel_mask,
	.set_channel_mask = zephyr_set_channel_mask,
};

static int zephyr_query_tx_possible(size_t size)
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

static int zephyr_info_is_busy(bool *busy)
{
	int rc = rzi_lorawan_zephyr_lock_started();

	if (rc != 0) {
		return rc;
	}
	*busy = rzi_lorawan_zephyr_busy();
	rzi_lorawan_zephyr_unlock();
	return 0;
}

static const struct rzi_lorawan_info_ops zephyr_info_ops = {
	.get_dev_nonce = zephyr_get_dev_nonce,
	.set_dev_nonce = zephyr_set_dev_nonce,
	.query_tx_possible = zephyr_query_tx_possible,
	.is_busy = zephyr_info_is_busy,
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

static const struct rzi_lorawan_link_check_ops zephyr_link_check_ops = {
	.request = zephyr_link_check_request,
};

static const struct rzi_lorawan_device_time_ops zephyr_device_time_ops = {
	.request = zephyr_device_time_request,
	.get_network_time = zephyr_get_network_time,
};

static const struct rzi_lorawan_backend_extension zephyr_network_ext = {
	.size = sizeof(zephyr_network_ops),
	.version = RZI_LORAWAN_NETWORK_OPS_VERSION,
	.api = &zephyr_network_ops,
};

static const struct rzi_lorawan_backend_extension zephyr_channel_ext = {
	.size = sizeof(zephyr_channel_ops),
	.version = RZI_LORAWAN_CHANNEL_OPS_VERSION,
	.api = &zephyr_channel_ops,
};

static const struct rzi_lorawan_backend_extension zephyr_info_ext = {
	.size = sizeof(zephyr_info_ops),
	.version = RZI_LORAWAN_INFO_OPS_VERSION,
	.api = &zephyr_info_ops,
};

static const struct rzi_lorawan_backend_extension zephyr_link_check_ext = {
	.size = sizeof(zephyr_link_check_ops),
	.version = RZI_LORAWAN_LINK_CHECK_OPS_VERSION,
	.api = &zephyr_link_check_ops,
};

static const struct rzi_lorawan_backend_extension zephyr_device_time_ext = {
	.size = sizeof(zephyr_device_time_ops),
	.version = RZI_LORAWAN_DEVICE_TIME_OPS_VERSION,
	.api = &zephyr_device_time_ops,
};

#ifdef CONFIG_RZI_LORAWAN_FUOTA
extern const struct rzi_lorawan_backend_extension rzi_lorawan_zephyr_fuota_extension;
#endif

void rzi_lorawan_zephyr_set_class(enum rzi_lorawan_class value)
{
	device_class = value;
}

const struct rzi_lorawan_backend_extension *
rzi_lorawan_zephyr_get_extension(enum rzi_lorawan_feature_id feature)
{
	switch (feature) {
	case RZI_LORAWAN_FEATURE_NETWORK_MANAGEMENT:
		return &zephyr_network_ext;
	case RZI_LORAWAN_FEATURE_CHANNEL_MANAGEMENT:
		return &zephyr_channel_ext;
	case RZI_LORAWAN_FEATURE_INFORMATION:
		return &zephyr_info_ext;
	case RZI_LORAWAN_FEATURE_LINK_CHECK:
		return &zephyr_link_check_ext;
	case RZI_LORAWAN_FEATURE_DEVICE_TIME:
		return &zephyr_device_time_ext;
#ifdef CONFIG_RZI_LORAWAN_FUOTA
	case RZI_LORAWAN_FEATURE_FUOTA:
		return &rzi_lorawan_zephyr_fuota_extension;
#endif
	default:
		return NULL;
	}
}
