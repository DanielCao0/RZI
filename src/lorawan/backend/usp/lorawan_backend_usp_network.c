/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief USP/LBM network-parameter and transmit-query operations.
 */

#include <errno.h>

#include "lorawan_backend_usp_priv.h"

#define LBT_DEFAULT_DURATION_MS 5U
#define LBT_DEFAULT_THRESHOLD   (-80)
#define LBT_DEFAULT_BW_HZ       200000U

static enum rzi_lorawan_data_rate cached_data_rate = RZI_LORAWAN_DR_0;

static int apply_custom_adr(enum rzi_lorawan_data_rate data_rate)
{
	uint8_t custom[SMTC_MODEM_CUSTOM_ADR_DATA_LENGTH] = {0};

	custom[data_rate] = 100U;
	return rzi_lorawan_usp_result(smtc_modem_adr_set_profile(
		RZI_LORAWAN_USP_STACK_ID, SMTC_MODEM_ADR_PROFILE_CUSTOM, custom));
}

static int usp_get_adr(bool *enabled)
{
	smtc_modem_adr_profile_t profile;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(smtc_modem_adr_get_profile(RZI_LORAWAN_USP_STACK_ID, &profile));
	if (rc == 0) {
		*enabled = profile == SMTC_MODEM_ADR_PROFILE_NETWORK_CONTROLLED;
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_set_adr(bool enabled)
{
	uint8_t custom[SMTC_MODEM_CUSTOM_ADR_DATA_LENGTH] = {0};
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	if (enabled) {
		rc = rzi_lorawan_usp_result(smtc_modem_adr_set_profile(
			RZI_LORAWAN_USP_STACK_ID, SMTC_MODEM_ADR_PROFILE_NETWORK_CONTROLLED,
			custom));
	} else {
		rc = apply_custom_adr(cached_data_rate);
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_get_data_rate(enum rzi_lorawan_data_rate *data_rate)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	*data_rate = cached_data_rate;
	return rzi_lorawan_usp_finish(0);
}

static int usp_set_data_rate(enum rzi_lorawan_data_rate data_rate)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = apply_custom_adr(data_rate);
	if (rc == 0) {
		cached_data_rate = data_rate;
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_get_public_network(bool *enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(
		smtc_modem_get_network_type(RZI_LORAWAN_USP_STACK_ID, enabled)));
}

static int usp_set_public_network(bool enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(
		smtc_modem_set_network_type(RZI_LORAWAN_USP_STACK_ID, enabled)));
}

static int usp_get_lbt(bool *enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(
		smtc_modem_lbt_get_state(RZI_LORAWAN_USP_STACK_ID, enabled)));
}

static int usp_set_lbt(bool enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	if (enabled) {
		rc = rzi_lorawan_usp_result(smtc_modem_lbt_set_parameters(
			RZI_LORAWAN_USP_STACK_ID, LBT_DEFAULT_DURATION_MS, LBT_DEFAULT_THRESHOLD,
			LBT_DEFAULT_BW_HZ));
		if (rc != 0) {
			return rzi_lorawan_usp_finish(rc);
		}
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(
		smtc_modem_lbt_set_state(RZI_LORAWAN_USP_STACK_ID, enabled)));
}

static int usp_get_lbt_rssi(int16_t *rssi_dbm)
{
	uint32_t duration_ms;
	uint32_t bw_hz;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(smtc_modem_lbt_get_parameters(
		RZI_LORAWAN_USP_STACK_ID, &duration_ms, rssi_dbm, &bw_hz)));
}

static int usp_set_lbt_rssi(int16_t rssi_dbm)
{
	uint32_t duration_ms;
	int16_t current;
	uint32_t bw_hz;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(smtc_modem_lbt_get_parameters(RZI_LORAWAN_USP_STACK_ID,
								  &duration_ms, &current, &bw_hz));
	if (rc != 0) {
		duration_ms = LBT_DEFAULT_DURATION_MS;
		bw_hz = LBT_DEFAULT_BW_HZ;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(smtc_modem_lbt_set_parameters(
		RZI_LORAWAN_USP_STACK_ID, duration_ms, rssi_dbm, bw_hz)));
}

static int usp_get_lbt_scan_time(uint32_t *time_ms)
{
	int16_t threshold;
	uint32_t bw_hz;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(smtc_modem_lbt_get_parameters(
		RZI_LORAWAN_USP_STACK_ID, time_ms, &threshold, &bw_hz)));
}

static int usp_set_lbt_scan_time(uint32_t time_ms)
{
	int16_t threshold;
	uint32_t duration_ms;
	uint32_t bw_hz;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(smtc_modem_lbt_get_parameters(
		RZI_LORAWAN_USP_STACK_ID, &duration_ms, &threshold, &bw_hz));
	if (rc != 0) {
		threshold = LBT_DEFAULT_THRESHOLD;
		bw_hz = LBT_DEFAULT_BW_HZ;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(smtc_modem_lbt_set_parameters(
		RZI_LORAWAN_USP_STACK_ID, time_ms, threshold, bw_hz)));
}

const struct rzi_lorawan_network_ops usp_network_ops = {
	.get_adr = usp_get_adr,
	.set_adr = usp_set_adr,
	.get_data_rate = usp_get_data_rate,
	.set_data_rate = usp_set_data_rate,
	.get_public_network = usp_get_public_network,
	.set_public_network = usp_set_public_network,
	.get_lbt = usp_get_lbt,
	.set_lbt = usp_set_lbt,
	.get_lbt_rssi = usp_get_lbt_rssi,
	.set_lbt_rssi = usp_set_lbt_rssi,
	.get_lbt_scan_time = usp_get_lbt_scan_time,
	.set_lbt_scan_time = usp_set_lbt_scan_time,
};

int usp_query_tx_possible(size_t size)
{
	uint8_t max_payload = 0;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(
		smtc_modem_get_next_tx_max_payload(RZI_LORAWAN_USP_STACK_ID, &max_payload));
	if (rc == 0 && size > max_payload) {
		rc = -RZI_ERR_TOO_LARGE;
	}
	return rzi_lorawan_usp_finish(rc);
}

int usp_is_busy(bool *busy)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	*busy = rzi_lorawan_usp_tx_pending();
	return rzi_lorawan_usp_finish(0);
}
