/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN channel-plan public implementation.
 */

#include <errno.h>

#include <rzi/lorawan/lorawan.h>

#include "../backend/lorawan_backend.h"
#include "../lorawan_priv.h"

static const struct rzi_lorawan_channel_ops *channel_ops(void)
{
	return rzi_lorawan_backend.channel;
}

int rzi_lorawan_get_channel_mask(uint16_t *mask, size_t words)
{
	const struct rzi_lorawan_channel_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (mask == NULL || words == 0U) {
		return -RZI_ERR_INVALID;
	}
	ops = channel_ops();
	if (ops == NULL || ops->get_channel_mask == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_channel_mask(mask, words);
}

int rzi_lorawan_set_channel_mask(const uint16_t *mask, size_t words)
{
	const struct rzi_lorawan_channel_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (mask == NULL || words == 0U || words > RZI_LORAWAN_CHANNEL_MASK_WORDS) {
		return -RZI_ERR_INVALID;
	}
	ops = channel_ops();
	if (ops == NULL || ops->set_channel_mask == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_channel_mask(mask, words);
}

int rzi_lorawan_get_sub_band(uint8_t *sub_band)
{
	const struct rzi_lorawan_channel_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (sub_band == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = channel_ops();
	if (ops == NULL || ops->get_sub_band == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_sub_band(sub_band);
}

int rzi_lorawan_set_sub_band(uint8_t sub_band)
{
	const struct rzi_lorawan_channel_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (sub_band > 8U) {
		return -RZI_ERR_INVALID;
	}
	ops = channel_ops();
	if (ops == NULL || ops->set_sub_band == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_sub_band(sub_band);
}

int rzi_lorawan_get_fixed_channel(uint32_t *frequency_hz)
{
	const struct rzi_lorawan_channel_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (frequency_hz == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = channel_ops();
	if (ops == NULL || ops->get_fixed_channel == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_fixed_channel(frequency_hz);
}

int rzi_lorawan_set_fixed_channel(uint32_t frequency_hz)
{
	const struct rzi_lorawan_channel_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = channel_ops();
	if (ops == NULL || ops->set_fixed_channel == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_fixed_channel(frequency_hz);
}
