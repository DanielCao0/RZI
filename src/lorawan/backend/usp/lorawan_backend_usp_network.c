/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief USP/LBM network-parameter, link-control and transmit-query operations.
 */

#include <errno.h>
#include <string.h>

#include "lorawan_backend_usp_priv.h"
#include "lorawan_backend_usp_lbm.h"

#define STACK_ID RZI_LORAWAN_USP_STACK_ID

#define LBT_DEFAULT_DURATION_MS 5U
#define LBT_DEFAULT_THRESHOLD   (-80)
#define LBT_DEFAULT_BW_HZ       200000U

BUILD_ASSERT(RZI_LORAWAN_DR_DISTRIBUTION_LENGTH == SMTC_MODEM_CUSTOM_ADR_DATA_LENGTH);
BUILD_ASSERT((int)RZI_LORAWAN_ADR_NETWORK == SMTC_MODEM_ADR_PROFILE_NETWORK_CONTROLLED &&
	     (int)RZI_LORAWAN_ADR_MOBILE_LONG_RANGE == SMTC_MODEM_ADR_PROFILE_MOBILE_LONG_RANGE &&
	     (int)RZI_LORAWAN_ADR_MOBILE_LOW_POWER == SMTC_MODEM_ADR_PROFILE_MOBILE_LOW_POWER &&
	     (int)RZI_LORAWAN_ADR_CUSTOM == SMTC_MODEM_ADR_PROFILE_CUSTOM);

/* With the lock held: LBM ADR profiles are only accepted with a session. */
static int set_profile(smtc_modem_adr_profile_t profile, const uint8_t *distribution)
{
	uint8_t custom[SMTC_MODEM_CUSTOM_ADR_DATA_LENGTH] = {0};
	int rc = rzi_lorawan_usp_require_joined();

	if (rc != 0) {
		return rc;
	}
	if (distribution != NULL) {
		memcpy(custom, distribution, sizeof(custom));
	}
	return rzi_usp_result(smtc_modem_adr_set_profile(STACK_ID, profile, custom));
}

/* A custom distribution whose every slot holds the same data rate pins it. */
static int pin_data_rate(uint8_t data_rate)
{
	uint8_t custom[SMTC_MODEM_CUSTOM_ADR_DATA_LENGTH];

	memset(custom, data_rate, sizeof(custom));
	return set_profile(SMTC_MODEM_ADR_PROFILE_CUSTOM, custom);
}

static int usp_get_adr(bool *enabled)
{
	smtc_modem_adr_profile_t profile;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_usp_result(smtc_modem_adr_get_profile(STACK_ID, &profile));
	if (rc == 0) {
		*enabled = profile == SMTC_MODEM_ADR_PROFILE_NETWORK_CONTROLLED;
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_set_adr(bool enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	if (enabled) {
		rc = set_profile(SMTC_MODEM_ADR_PROFILE_NETWORK_CONTROLLED, NULL);
	} else {
		/* Keep transmitting at the data rate ADR last selected. */
		rc = pin_data_rate(lorawan_api_next_dr_get(STACK_ID));
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_get_data_rate(enum rzi_lorawan_data_rate *data_rate)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	*data_rate = (enum rzi_lorawan_data_rate)lorawan_api_next_dr_get(STACK_ID);
	return rzi_lorawan_usp_finish(0);
}

static int usp_set_data_rate(enum rzi_lorawan_data_rate data_rate)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(pin_data_rate((uint8_t)data_rate));
}

static int usp_get_enabled_data_rates(uint16_t *mask)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(
		rzi_usp_result(smtc_modem_get_enabled_datarates(STACK_ID, mask)));
}

static int usp_get_adr_profile(enum rzi_lorawan_adr_profile *profile)
{
	smtc_modem_adr_profile_t value;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_usp_result(smtc_modem_adr_get_profile(STACK_ID, &value));
	if (rc == 0) {
		*profile = (enum rzi_lorawan_adr_profile)value;
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_set_adr_profile(enum rzi_lorawan_adr_profile profile, const uint8_t *distribution)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(
		set_profile((smtc_modem_adr_profile_t)profile,
			    profile == RZI_LORAWAN_ADR_CUSTOM ? distribution : NULL));
}

static int usp_get_nb_trans(uint8_t *nb_trans)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_usp_result(smtc_modem_get_nb_trans(STACK_ID, nb_trans)));
}

static int usp_set_nb_trans(uint8_t nb_trans)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_require_joined();
	if (rc == 0) {
		rc = rzi_usp_result(smtc_modem_set_nb_trans(STACK_ID, nb_trans));
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_get_adr_ack(uint8_t *limit, uint8_t *delay)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(
		rzi_usp_result(smtc_modem_get_adr_ack_limit_delay(STACK_ID, limit, delay)));
}

static int usp_set_adr_ack(uint8_t limit, uint8_t delay)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(
		rzi_usp_result(smtc_modem_set_adr_ack_limit_delay(STACK_ID, limit, delay)));
}

static int usp_set_join_data_rates(const uint8_t *distribution)
{
	smtc_modem_status_mask_t status = 0;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_usp_result(smtc_modem_get_status(STACK_ID, &status));
	if (rc == 0 && (status & (SMTC_MODEM_STATUS_JOINED | SMTC_MODEM_STATUS_JOINING)) != 0U) {
		rc = -RZI_ERR_BUSY;
	}
	if (rc == 0) {
		rc = rzi_usp_result(smtc_modem_adr_set_join_distribution(STACK_ID, distribution));
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_get_duty_cycle_status(int32_t *time_ms)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(
		rzi_usp_result(smtc_modem_get_duty_cycle_status(STACK_ID, time_ms)));
}

static int usp_get_public_network(bool *enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(
		rzi_usp_result(smtc_modem_get_network_type(STACK_ID, enabled)));
}

static int usp_set_public_network(bool enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(
		rzi_usp_result(smtc_modem_set_network_type(STACK_ID, enabled)));
}

static int usp_get_lbt(bool *enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_usp_result(smtc_modem_lbt_get_state(STACK_ID, enabled)));
}

static int usp_set_lbt(bool enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	if (enabled) {
		rc = rzi_usp_result(smtc_modem_lbt_set_parameters(STACK_ID, LBT_DEFAULT_DURATION_MS,
								  LBT_DEFAULT_THRESHOLD,
								  LBT_DEFAULT_BW_HZ));
		if (rc != 0) {
			return rzi_lorawan_usp_finish(rc);
		}
	}
	return rzi_lorawan_usp_finish(rzi_usp_result(smtc_modem_lbt_set_state(STACK_ID, enabled)));
}

static int usp_get_lbt_rssi(int16_t *rssi_dbm)
{
	uint32_t duration_ms;
	uint32_t bw_hz;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_usp_result(
		smtc_modem_lbt_get_parameters(STACK_ID, &duration_ms, rssi_dbm, &bw_hz)));
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
	rc = rzi_usp_result(
		smtc_modem_lbt_get_parameters(STACK_ID, &duration_ms, &current, &bw_hz));
	if (rc != 0) {
		duration_ms = LBT_DEFAULT_DURATION_MS;
		bw_hz = LBT_DEFAULT_BW_HZ;
	}
	return rzi_lorawan_usp_finish(rzi_usp_result(
		smtc_modem_lbt_set_parameters(STACK_ID, duration_ms, rssi_dbm, bw_hz)));
}

static int usp_get_lbt_scan_time(uint32_t *time_ms)
{
	int16_t threshold;
	uint32_t bw_hz;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_usp_result(
		smtc_modem_lbt_get_parameters(STACK_ID, time_ms, &threshold, &bw_hz)));
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
	rc = rzi_usp_result(
		smtc_modem_lbt_get_parameters(STACK_ID, &duration_ms, &threshold, &bw_hz));
	if (rc != 0) {
		threshold = LBT_DEFAULT_THRESHOLD;
		bw_hz = LBT_DEFAULT_BW_HZ;
	}
	return rzi_lorawan_usp_finish(
		rzi_usp_result(smtc_modem_lbt_set_parameters(STACK_ID, time_ms, threshold, bw_hz)));
}

#ifdef CONFIG_LORA_BASICS_MODEM_CSMA
static int usp_get_csma(bool *enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_usp_result(smtc_modem_csma_get_state(STACK_ID, enabled)));
}

static int usp_set_csma(bool enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_usp_result(smtc_modem_csma_set_state(STACK_ID, enabled)));
}

static int usp_get_csma_params(struct rzi_lorawan_csma_params *params)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_usp_result(
		smtc_modem_csma_get_parameters(STACK_ID, &params->max_channel_changes,
					       &params->backoff_enabled, &params->max_backoffs)));
}

static int usp_set_csma_params(const struct rzi_lorawan_csma_params *params)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_usp_result(
		smtc_modem_csma_set_parameters(STACK_ID, params->max_channel_changes,
					       params->backoff_enabled, params->max_backoffs)));
}
#endif

static int usp_get_lost_connection_count(uint16_t *count)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(
		rzi_usp_result(smtc_modem_lorawan_get_lost_connection_counter(STACK_ID, count)));
}

static int usp_get_no_downlink_threshold(uint16_t *uplinks)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	*uplinks = lorawan_api_get_no_rx_packet_threshold(STACK_ID);
	return rzi_lorawan_usp_finish(0);
}

static int usp_set_no_downlink_threshold(uint16_t uplinks)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	lorawan_api_set_no_rx_packet_threshold(uplinks, STACK_ID);
	return rzi_lorawan_usp_finish(0);
}

static int usp_get_crystal_error(uint32_t *ppm)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_usp_result(smtc_modem_get_crystal_error_ppm(ppm)));
}

static int usp_set_crystal_error(uint32_t ppm)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_usp_result(smtc_modem_set_crystal_error_ppm(ppm)));
}

static int usp_send_empty(bool with_port, uint8_t port, enum rzi_lorawan_message_type type)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	if (rzi_lorawan_usp_tx_pending()) {
		return rzi_lorawan_usp_finish(-RZI_ERR_BUSY);
	}
	rc = rzi_usp_result(smtc_modem_request_empty_uplink(STACK_ID, with_port, port,
							    type == RZI_LORAWAN_MSG_CONFIRMED));
	if (rc == 0) {
		rzi_lorawan_usp_set_tx_pending();
	}
	return rzi_lorawan_usp_finish(rc);
}

const struct rzi_lorawan_network_ops usp_network_ops = {
	.get_adr = usp_get_adr,
	.set_adr = usp_set_adr,
	.get_data_rate = usp_get_data_rate,
	.set_data_rate = usp_set_data_rate,
	.get_enabled_data_rates = usp_get_enabled_data_rates,
	.get_adr_profile = usp_get_adr_profile,
	.set_adr_profile = usp_set_adr_profile,
	.get_nb_trans = usp_get_nb_trans,
	.set_nb_trans = usp_set_nb_trans,
	.get_adr_ack = usp_get_adr_ack,
	.set_adr_ack = usp_set_adr_ack,
	.set_join_data_rates = usp_set_join_data_rates,
	.get_duty_cycle_status = usp_get_duty_cycle_status,
	.get_public_network = usp_get_public_network,
	.set_public_network = usp_set_public_network,
	.get_lbt = usp_get_lbt,
	.set_lbt = usp_set_lbt,
	.get_lbt_rssi = usp_get_lbt_rssi,
	.set_lbt_rssi = usp_set_lbt_rssi,
	.get_lbt_scan_time = usp_get_lbt_scan_time,
	.set_lbt_scan_time = usp_set_lbt_scan_time,
#ifdef CONFIG_LORA_BASICS_MODEM_CSMA
	.get_csma = usp_get_csma,
	.set_csma = usp_set_csma,
	.get_csma_params = usp_get_csma_params,
	.set_csma_params = usp_set_csma_params,
#endif
	.get_lost_connection_count = usp_get_lost_connection_count,
	.get_no_downlink_threshold = usp_get_no_downlink_threshold,
	.set_no_downlink_threshold = usp_set_no_downlink_threshold,
	.get_crystal_error = usp_get_crystal_error,
	.set_crystal_error = usp_set_crystal_error,
	.send_empty = usp_send_empty,
};

int usp_query_tx_possible(size_t size)
{
	uint8_t max_payload = 0;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_usp_result(smtc_modem_get_next_tx_max_payload(STACK_ID, &max_payload));
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
