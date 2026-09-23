/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN network-parameter public implementation.
 */

#include <errno.h>

#include <rzi/lorawan/lorawan.h>

#include "../backend/lorawan_backend.h"
#include "../lorawan_priv.h"

static const struct rzi_lorawan_network_ops *network_ops(void)
{
	return rzi_lorawan_backend.network;
}

/* RP2 / LBM region tables. Index is the 4-bit MAC DR; bits are legal values. */
static uint16_t uplink_data_rate_mask(enum rzi_lorawan_region region)
{
	switch (region) {
	case RZI_LORAWAN_REGION_EU_868:
		return 0x0fffU; /* DR0-11 (DR8-11 are LR-FHSS) */
	case RZI_LORAWAN_REGION_US_915:
		return 0x007fU; /* DR0-6 */
	case RZI_LORAWAN_REGION_AU_915:
		return 0x00ffU; /* DR0-7 */
	case RZI_LORAWAN_REGION_CN_470:
	case RZI_LORAWAN_REGION_AS_923_GRP1:
	case RZI_LORAWAN_REGION_AS_923_GRP2:
	case RZI_LORAWAN_REGION_AS_923_GRP3:
	case RZI_LORAWAN_REGION_AS_923_GRP4:
	case RZI_LORAWAN_REGION_RU_864:
		return 0x00ffU; /* DR0-7 */
	case RZI_LORAWAN_REGION_IN_865:
		return 0x00bfU; /* DR0-5, DR7 (DR6 is RFU) */
	case RZI_LORAWAN_REGION_KR_920:
		return 0x003fU; /* DR0-5 */
	default:
		return 0U;
	}
}

static uint16_t rx_data_rate_mask(enum rzi_lorawan_region region)
{
	switch (region) {
	case RZI_LORAWAN_REGION_US_915:
	case RZI_LORAWAN_REGION_AU_915:
		return 0x3f00U; /* DR8-13 */
	case RZI_LORAWAN_REGION_IN_865:
		return 0x00bfU; /* DR0-5, DR7 */
	case RZI_LORAWAN_REGION_KR_920:
		return 0x003fU; /* DR0-5 */
	case RZI_LORAWAN_REGION_EU_868:
	case RZI_LORAWAN_REGION_CN_470:
	case RZI_LORAWAN_REGION_AS_923_GRP1:
	case RZI_LORAWAN_REGION_AS_923_GRP2:
	case RZI_LORAWAN_REGION_AS_923_GRP3:
	case RZI_LORAWAN_REGION_AS_923_GRP4:
	case RZI_LORAWAN_REGION_RU_864:
		return 0x00ffU; /* DR0-7 */
	default:
		return 0U;
	}
}

static bool data_rate_in_mask(enum rzi_lorawan_data_rate data_rate, uint16_t mask)
{
	unsigned int index = (unsigned int)data_rate;

	return index <= (unsigned int)RZI_LORAWAN_DR_15 && (mask & (1U << index)) != 0U;
}

bool rzi_lorawan_uplink_data_rate_valid(enum rzi_lorawan_data_rate data_rate)
{
	enum rzi_lorawan_region region;

	if (rzi_lorawan_get_region(&region) != 0) {
		return false;
	}
	return data_rate_in_mask(data_rate, uplink_data_rate_mask(region));
}

bool rzi_lorawan_rx_data_rate_valid(enum rzi_lorawan_data_rate data_rate)
{
	enum rzi_lorawan_region region;

	if (rzi_lorawan_get_region(&region) != 0) {
		return false;
	}
	return data_rate_in_mask(data_rate, rx_data_rate_mask(region));
}

int rzi_lorawan_get_adr(bool *enabled)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (enabled == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_adr == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_adr(enabled);
}

int rzi_lorawan_set_adr(bool enabled)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_adr == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_adr(enabled);
}

int rzi_lorawan_get_data_rate(enum rzi_lorawan_data_rate *data_rate)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (data_rate == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_data_rate == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_data_rate(data_rate);
}

int rzi_lorawan_set_data_rate(enum rzi_lorawan_data_rate data_rate)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (!rzi_lorawan_uplink_data_rate_valid(data_rate)) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_data_rate == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_data_rate(data_rate);
}

int rzi_lorawan_get_tx_power(uint8_t *tx_power)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (tx_power == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_tx_power == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_tx_power(tx_power);
}

int rzi_lorawan_set_tx_power(uint8_t tx_power)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (tx_power > 15U) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_tx_power == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_tx_power(tx_power);
}

int rzi_lorawan_get_duty_cycle(bool *enabled)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (enabled == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_duty_cycle == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_duty_cycle(enabled);
}

int rzi_lorawan_set_duty_cycle(bool enabled)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_duty_cycle == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_duty_cycle(enabled);
}

int rzi_lorawan_get_rx1_delay(uint32_t *delay_ms)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (delay_ms == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_rx1_delay == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_rx1_delay(delay_ms);
}

int rzi_lorawan_set_rx1_delay(uint32_t delay_ms)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (delay_ms == 0U) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_rx1_delay == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_rx1_delay(delay_ms);
}

int rzi_lorawan_get_rx2_delay(uint32_t *delay_ms)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (delay_ms == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_rx2_delay == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_rx2_delay(delay_ms);
}

int rzi_lorawan_set_rx2_delay(uint32_t delay_ms)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (delay_ms == 0U) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_rx2_delay == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_rx2_delay(delay_ms);
}

int rzi_lorawan_get_rx2_data_rate(enum rzi_lorawan_data_rate *data_rate)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (data_rate == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_rx2_data_rate == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_rx2_data_rate(data_rate);
}

int rzi_lorawan_set_rx2_data_rate(enum rzi_lorawan_data_rate data_rate)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (!rzi_lorawan_rx_data_rate_valid(data_rate)) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_rx2_data_rate == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_rx2_data_rate(data_rate);
}

int rzi_lorawan_get_rx2_frequency(uint32_t *frequency_hz)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (frequency_hz == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_rx2_frequency == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_rx2_frequency(frequency_hz);
}

int rzi_lorawan_set_rx2_frequency(uint32_t frequency_hz)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (frequency_hz == 0U) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_rx2_frequency == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_rx2_frequency(frequency_hz);
}

int rzi_lorawan_get_join_accept_delay1(uint32_t *delay_ms)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (delay_ms == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_join_accept_delay1 == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_join_accept_delay1(delay_ms);
}

int rzi_lorawan_set_join_accept_delay1(uint32_t delay_ms)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (delay_ms == 0U) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_join_accept_delay1 == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_join_accept_delay1(delay_ms);
}

int rzi_lorawan_get_join_accept_delay2(uint32_t *delay_ms)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (delay_ms == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_join_accept_delay2 == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_join_accept_delay2(delay_ms);
}

int rzi_lorawan_set_join_accept_delay2(uint32_t delay_ms)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (delay_ms == 0U) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_join_accept_delay2 == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_join_accept_delay2(delay_ms);
}

int rzi_lorawan_get_public_network(bool *enabled)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (enabled == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_public_network == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_public_network(enabled);
}

int rzi_lorawan_set_public_network(bool enabled)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_public_network == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_public_network(enabled);
}

int rzi_lorawan_get_lbt(bool *enabled)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (enabled == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_lbt == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_lbt(enabled);
}

int rzi_lorawan_set_lbt(bool enabled)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_lbt == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_lbt(enabled);
}

int rzi_lorawan_get_lbt_rssi(int16_t *rssi_dbm)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (rssi_dbm == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_lbt_rssi == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_lbt_rssi(rssi_dbm);
}

int rzi_lorawan_set_lbt_rssi(int16_t rssi_dbm)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_lbt_rssi == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_lbt_rssi(rssi_dbm);
}

int rzi_lorawan_get_lbt_scan_time(uint32_t *time_ms)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (time_ms == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_lbt_scan_time == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_lbt_scan_time(time_ms);
}

int rzi_lorawan_set_lbt_scan_time(uint32_t time_ms)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (time_ms == 0U) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_lbt_scan_time == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_lbt_scan_time(time_ms);
}

int rzi_lorawan_query_tx_possible(size_t size)
{
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (rzi_lorawan_backend.query_tx_possible == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return rzi_lorawan_backend.query_tx_possible(size);
}
