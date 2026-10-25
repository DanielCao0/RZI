/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN network-parameter and link-control public implementation.
 */

#include <errno.h>

#include <zephyr/sys/util.h>

#include <rzi/lorawan/lorawan.h>

#include "../backend/lorawan_backend.h"
#include "../lorawan_priv.h"

/* Resolves to NULL when the backend has no network table. */
#define NETWORK_OP(member)                                                                         \
	(rzi_lorawan_backend.network != NULL ? rzi_lorawan_backend.network->member : NULL)

/* Started-check, then call the op or report it unsupported. */
#define CALL_NETWORK_OP(member, ...)                                                               \
	do {                                                                                       \
		int rc_ = rzi_lorawan_check_started();                                             \
                                                                                                   \
		if (rc_ != 0) {                                                                    \
			return rc_;                                                                \
		}                                                                                  \
		if (NETWORK_OP(member) == NULL) {                                                  \
			return -RZI_ERR_NOT_SUPPORTED;                                             \
		}                                                                                  \
		return NETWORK_OP(member)(__VA_ARGS__);                                            \
	} while (false)

/* RUI3 parameters no current backend implements; see lorawan.h. */
static int unsupported(void)
{
	int rc = rzi_lorawan_check_started();

	return rc != 0 ? rc : -RZI_ERR_NOT_SUPPORTED;
}

static int check_uplink_data_rate(enum rzi_lorawan_data_rate data_rate)
{
	uint16_t mask = 0;
	int rc;

	if ((unsigned int)data_rate > RZI_LORAWAN_DR_15) {
		return -RZI_ERR_INVALID;
	}
	if (NETWORK_OP(get_enabled_data_rates) == NULL) {
		return 0;
	}
	rc = NETWORK_OP(get_enabled_data_rates)(&mask);
	if (rc != 0) {
		return rc;
	}
	return (mask & (1U << data_rate)) != 0U ? 0 : -RZI_ERR_INVALID;
}

static bool distribution_valid(const uint8_t *distribution)
{
	if (distribution == NULL) {
		return false;
	}
	for (size_t i = 0; i < RZI_LORAWAN_DR_DISTRIBUTION_LENGTH; ++i) {
		if (distribution[i] > 15U) {
			return false;
		}
	}
	return true;
}

int rzi_lorawan_get_adr(bool *enabled)
{
	if (enabled == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_adr, enabled);
}

int rzi_lorawan_set_adr(bool enabled)
{
	CALL_NETWORK_OP(set_adr, enabled);
}

int rzi_lorawan_get_data_rate(enum rzi_lorawan_data_rate *data_rate)
{
	if (data_rate == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_data_rate, data_rate);
}

int rzi_lorawan_set_data_rate(enum rzi_lorawan_data_rate data_rate)
{
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (NETWORK_OP(set_data_rate) == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	rc = check_uplink_data_rate(data_rate);
	if (rc != 0) {
		return rc;
	}
	return NETWORK_OP(set_data_rate)(data_rate);
}

int rzi_lorawan_get_tx_power(uint8_t *tx_power)
{
	ARG_UNUSED(tx_power);
	return unsupported();
}

int rzi_lorawan_set_tx_power(uint8_t tx_power)
{
	ARG_UNUSED(tx_power);
	return unsupported();
}

int rzi_lorawan_get_duty_cycle(bool *enabled)
{
	ARG_UNUSED(enabled);
	return unsupported();
}

int rzi_lorawan_set_duty_cycle(bool enabled)
{
	ARG_UNUSED(enabled);
	return unsupported();
}

int rzi_lorawan_get_rx1_delay(uint32_t *delay_ms)
{
	ARG_UNUSED(delay_ms);
	return unsupported();
}

int rzi_lorawan_set_rx1_delay(uint32_t delay_ms)
{
	ARG_UNUSED(delay_ms);
	return unsupported();
}

int rzi_lorawan_get_rx2_delay(uint32_t *delay_ms)
{
	ARG_UNUSED(delay_ms);
	return unsupported();
}

int rzi_lorawan_set_rx2_delay(uint32_t delay_ms)
{
	ARG_UNUSED(delay_ms);
	return unsupported();
}

int rzi_lorawan_get_rx2_data_rate(enum rzi_lorawan_data_rate *data_rate)
{
	ARG_UNUSED(data_rate);
	return unsupported();
}

int rzi_lorawan_set_rx2_data_rate(enum rzi_lorawan_data_rate data_rate)
{
	ARG_UNUSED(data_rate);
	return unsupported();
}

int rzi_lorawan_get_rx2_frequency(uint32_t *frequency_hz)
{
	ARG_UNUSED(frequency_hz);
	return unsupported();
}

int rzi_lorawan_set_rx2_frequency(uint32_t frequency_hz)
{
	ARG_UNUSED(frequency_hz);
	return unsupported();
}

int rzi_lorawan_get_join_accept_delay1(uint32_t *delay_ms)
{
	ARG_UNUSED(delay_ms);
	return unsupported();
}

int rzi_lorawan_set_join_accept_delay1(uint32_t delay_ms)
{
	ARG_UNUSED(delay_ms);
	return unsupported();
}

int rzi_lorawan_get_join_accept_delay2(uint32_t *delay_ms)
{
	ARG_UNUSED(delay_ms);
	return unsupported();
}

int rzi_lorawan_set_join_accept_delay2(uint32_t delay_ms)
{
	ARG_UNUSED(delay_ms);
	return unsupported();
}

int rzi_lorawan_get_public_network(bool *enabled)
{
	if (enabled == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_public_network, enabled);
}

int rzi_lorawan_set_public_network(bool enabled)
{
	CALL_NETWORK_OP(set_public_network, enabled);
}

int rzi_lorawan_get_lbt(bool *enabled)
{
	if (enabled == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_lbt, enabled);
}

int rzi_lorawan_set_lbt(bool enabled)
{
	CALL_NETWORK_OP(set_lbt, enabled);
}

int rzi_lorawan_get_lbt_rssi(int16_t *rssi_dbm)
{
	if (rssi_dbm == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_lbt_rssi, rssi_dbm);
}

int rzi_lorawan_set_lbt_rssi(int16_t rssi_dbm)
{
	CALL_NETWORK_OP(set_lbt_rssi, rssi_dbm);
}

int rzi_lorawan_get_lbt_scan_time(uint32_t *time_ms)
{
	if (time_ms == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_lbt_scan_time, time_ms);
}

int rzi_lorawan_set_lbt_scan_time(uint32_t time_ms)
{
	if (time_ms == 0U) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(set_lbt_scan_time, time_ms);
}

int rzi_lorawan_get_adr_profile(enum rzi_lorawan_adr_profile *profile)
{
	if (profile == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_adr_profile, profile);
}

int rzi_lorawan_set_adr_profile(enum rzi_lorawan_adr_profile profile, const uint8_t *distribution)
{
	if ((unsigned int)profile > RZI_LORAWAN_ADR_CUSTOM ||
	    (profile == RZI_LORAWAN_ADR_CUSTOM && !distribution_valid(distribution))) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(set_adr_profile, profile, distribution);
}

int rzi_lorawan_get_nb_trans(uint8_t *nb_trans)
{
	if (nb_trans == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_nb_trans, nb_trans);
}

int rzi_lorawan_set_nb_trans(uint8_t nb_trans)
{
	if (nb_trans == 0U || nb_trans > 15U) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(set_nb_trans, nb_trans);
}

int rzi_lorawan_get_adr_ack(uint8_t *limit, uint8_t *delay)
{
	if (limit == NULL || delay == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_adr_ack, limit, delay);
}

int rzi_lorawan_set_adr_ack(uint8_t limit, uint8_t delay)
{
	if (limit < 2U || limit > 127U || delay < 2U || delay > 127U) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(set_adr_ack, limit, delay);
}

int rzi_lorawan_set_join_data_rates(const uint8_t *distribution)
{
	if (!distribution_valid(distribution)) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(set_join_data_rates, distribution);
}

int rzi_lorawan_get_join_backoff(uint32_t *remaining_s)
{
	if (remaining_s == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_join_backoff, remaining_s);
}

int rzi_lorawan_get_duty_cycle_status(int32_t *time_ms)
{
	if (time_ms == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_duty_cycle_status, time_ms);
}

int rzi_lorawan_get_csma(bool *enabled)
{
	if (enabled == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_csma, enabled);
}

int rzi_lorawan_set_csma(bool enabled)
{
	CALL_NETWORK_OP(set_csma, enabled);
}

int rzi_lorawan_get_csma_params(struct rzi_lorawan_csma_params *params)
{
	if (params == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_csma_params, params);
}

int rzi_lorawan_set_csma_params(const struct rzi_lorawan_csma_params *params)
{
	if (params == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(set_csma_params, params);
}

int rzi_lorawan_get_lost_connection_count(uint16_t *count)
{
	if (count == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_lost_connection_count, count);
}

int rzi_lorawan_get_no_downlink_threshold(uint16_t *uplinks)
{
	if (uplinks == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_no_downlink_threshold, uplinks);
}

int rzi_lorawan_set_no_downlink_threshold(uint16_t uplinks)
{
	CALL_NETWORK_OP(set_no_downlink_threshold, uplinks);
}

int rzi_lorawan_get_crystal_error(uint32_t *ppm)
{
	if (ppm == NULL) {
		return -RZI_ERR_INVALID;
	}
	CALL_NETWORK_OP(get_crystal_error, ppm);
}

int rzi_lorawan_set_crystal_error(uint32_t ppm)
{
	CALL_NETWORK_OP(set_crystal_error, ppm);
}

int rzi_lorawan_query_tx_possible(size_t size)
{
	CALL_NETWORK_OP(query_tx_possible, size);
}
