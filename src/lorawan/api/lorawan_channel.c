/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN channel-plan public implementation.
 *
 * The core owns one channel mask and passes it to the backend in the join
 * configuration. Sub-band and fixed-channel selections are views of it.
 */

#include <string.h>

#include <zephyr/sys/util.h>

#include <rzi/lorawan/lorawan.h>

#include "../backend/lorawan_backend.h"
#include "../lorawan_priv.h"

#define NARROW_STEP_HZ 200000U
#define WIDE_STEP_HZ   1600000U
#define SUB_BANDS      8U

struct channel_plan {
	uint32_t narrow_base_hz;
	uint32_t wide_base_hz;
};

bool rzi_lorawan_region_has_fixed_plan(enum rzi_lorawan_region region)
{
	return region == RZI_LORAWAN_REGION_US_915 || region == RZI_LORAWAN_REGION_AU_915;
}

static int channel_plan(struct channel_plan *plan)
{
	enum rzi_lorawan_region region;
	int rc = rzi_lorawan_get_region(&region);

	if (rc != 0) {
		return rc;
	}
	if ((rzi_lorawan_backend.capabilities & RZI_LORAWAN_CAP_CHANNEL) == 0U) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	if (region == RZI_LORAWAN_REGION_US_915) {
		*plan = (struct channel_plan){902300000U, 903000000U};
	} else if (region == RZI_LORAWAN_REGION_AU_915) {
		*plan = (struct channel_plan){915200000U, 915900000U};
	} else {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return 0;
}

static void put_channel(uint16_t *mask, uint32_t channel)
{
	mask[channel / 16U] |= BIT(channel % 16U);
}

static bool has_channel(const uint16_t *mask, uint32_t channel)
{
	return (mask[channel / 16U] & BIT(channel % 16U)) != 0U;
}

static void all_channels(uint16_t *mask)
{
	memset(mask, 0, RZI_LORAWAN_CHANNEL_MASK_WORDS * sizeof(mask[0]));
	for (uint32_t channel = 0; channel < RZI_LORAWAN_FIXED_PLAN_CHANNELS; ++channel) {
		put_channel(mask, channel);
	}
}

/* LoRaWAN FSB n: eight 125 kHz channels and the matching 500 kHz channel. */
static void sub_band_channels(uint8_t sub_band, uint16_t *mask)
{
	uint32_t first = (sub_band - 1U) * 8U;

	memset(mask, 0, RZI_LORAWAN_CHANNEL_MASK_WORDS * sizeof(mask[0]));
	for (uint32_t channel = first; channel < first + 8U; ++channel) {
		put_channel(mask, channel);
	}
	put_channel(mask, RZI_LORAWAN_FIXED_PLAN_WIDE + sub_band - 1U);
}

static bool same_mask(const uint16_t *a, const uint16_t *b)
{
	return memcmp(a, b, RZI_LORAWAN_CHANNEL_MASK_WORDS * sizeof(a[0])) == 0;
}

static int read_mask(uint16_t *mask)
{
	bool set = false;
	int rc = rzi_lorawan_credentials_get_channel_mask(mask, &set);

	if (rc == 0 && !set) {
		all_channels(mask);
	}
	return rc;
}

static int store_mask(const uint16_t *mask)
{
	uint16_t full[RZI_LORAWAN_CHANNEL_MASK_WORDS];

	all_channels(full);
	return rzi_lorawan_credentials_set_channel_mask(same_mask(mask, full) ? NULL : mask);
}

static uint32_t channel_frequency(const struct channel_plan *plan, uint32_t channel)
{
	if (channel < RZI_LORAWAN_FIXED_PLAN_WIDE) {
		return plan->narrow_base_hz + channel * NARROW_STEP_HZ;
	}
	return plan->wide_base_hz + (channel - RZI_LORAWAN_FIXED_PLAN_WIDE) * WIDE_STEP_HZ;
}

static int frequency_channel(const struct channel_plan *plan, uint32_t frequency_hz)
{
	for (uint32_t channel = 0; channel < RZI_LORAWAN_FIXED_PLAN_CHANNELS; ++channel) {
		if (channel_frequency(plan, channel) == frequency_hz) {
			return (int)channel;
		}
	}
	return -RZI_ERR_INVALID;
}

int rzi_lorawan_get_channel_mask(uint16_t *mask, size_t words)
{
	uint16_t current[RZI_LORAWAN_CHANNEL_MASK_WORDS];
	struct channel_plan plan;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (mask == NULL || words == 0U) {
		return -RZI_ERR_INVALID;
	}
	rc = channel_plan(&plan);
	if (rc != 0) {
		return rc;
	}
	if (words < RZI_LORAWAN_FIXED_PLAN_WORDS) {
		return -RZI_ERR_INVALID;
	}
	rc = read_mask(current);
	if (rc != 0) {
		return rc;
	}
	for (size_t word = 0; word < words; ++word) {
		mask[word] = word < ARRAY_SIZE(current) ? current[word] : 0U;
	}
	return 0;
}

int rzi_lorawan_set_channel_mask(const uint16_t *mask, size_t words)
{
	uint16_t requested[RZI_LORAWAN_CHANNEL_MASK_WORDS] = {0};
	struct channel_plan plan;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (mask == NULL || words == 0U || words > RZI_LORAWAN_CHANNEL_MASK_WORDS) {
		return -RZI_ERR_INVALID;
	}
	rc = channel_plan(&plan);
	if (rc != 0) {
		return rc;
	}
	if (words < RZI_LORAWAN_FIXED_PLAN_WORDS) {
		return -RZI_ERR_INVALID;
	}
	memcpy(requested, mask, words * sizeof(mask[0]));
	return store_mask(requested);
}

int rzi_lorawan_get_sub_band(uint8_t *sub_band)
{
	uint16_t current[RZI_LORAWAN_CHANNEL_MASK_WORDS];
	uint16_t candidate[RZI_LORAWAN_CHANNEL_MASK_WORDS];
	struct channel_plan plan;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (sub_band == NULL) {
		return -RZI_ERR_INVALID;
	}
	rc = channel_plan(&plan);
	if (rc == 0) {
		rc = read_mask(current);
	}
	if (rc != 0) {
		return rc;
	}
	all_channels(candidate);
	if (same_mask(current, candidate)) {
		*sub_band = 0U;
		return 0;
	}
	for (uint8_t band = 1U; band <= SUB_BANDS; ++band) {
		sub_band_channels(band, candidate);
		if (same_mask(current, candidate)) {
			*sub_band = band;
			return 0;
		}
	}
	return -RZI_ERR_NO_DATA;
}

int rzi_lorawan_set_sub_band(uint8_t sub_band)
{
	uint16_t requested[RZI_LORAWAN_CHANNEL_MASK_WORDS];
	struct channel_plan plan;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	rc = channel_plan(&plan);
	if (rc != 0) {
		return rc;
	}
	if (sub_band > SUB_BANDS) {
		return -RZI_ERR_INVALID;
	}
	if (sub_band == 0U) {
		return rzi_lorawan_credentials_set_channel_mask(NULL);
	}
	sub_band_channels(sub_band, requested);
	return store_mask(requested);
}

int rzi_lorawan_get_fixed_channel(uint32_t *frequency_hz)
{
	uint16_t current[RZI_LORAWAN_CHANNEL_MASK_WORDS];
	struct channel_plan plan;
	uint32_t found = 0U;
	uint32_t count = 0U;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (frequency_hz == NULL) {
		return -RZI_ERR_INVALID;
	}
	rc = channel_plan(&plan);
	if (rc == 0) {
		rc = read_mask(current);
	}
	if (rc != 0) {
		return rc;
	}
	for (uint32_t channel = 0; channel < RZI_LORAWAN_FIXED_PLAN_CHANNELS; ++channel) {
		if (has_channel(current, channel)) {
			found = channel;
			count++;
		}
	}
	*frequency_hz = count == 1U ? channel_frequency(&plan, found) : 0U;
	return 0;
}

int rzi_lorawan_set_fixed_channel(uint32_t frequency_hz)
{
	uint16_t requested[RZI_LORAWAN_CHANNEL_MASK_WORDS] = {0};
	struct channel_plan plan;
	int channel;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	rc = channel_plan(&plan);
	if (rc != 0) {
		return rc;
	}
	if (frequency_hz == 0U) {
		return rzi_lorawan_credentials_set_channel_mask(NULL);
	}
	channel = frequency_channel(&plan, frequency_hz);
	if (channel < 0) {
		return channel;
	}
	put_channel(requested, (uint32_t)channel);
	return rzi_lorawan_credentials_set_channel_mask(requested);
}
