/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN network, channel, and session-information APIs.
 */

#include <errno.h>

#include <rzi/lorawan/lorawan.h>

#include "../backend/lorawan_channel.h"
#include "../backend/lorawan_feature.h"
#include "../backend/lorawan_info.h"
#include "../backend/lorawan_network.h"
#include "lorawan_priv.h"

static const struct rzi_lorawan_network_ops *network_ops(void)
{
	if ((rzi_lorawan_get_capabilities() &
	     (RZI_LORAWAN_CAP_NETWORK_MANAGEMENT | RZI_LORAWAN_CAP_CLASS_B)) == 0U) {
		return NULL;
	}
	return rzi_lorawan_feature_ops(RZI_LORAWAN_FEATURE_NETWORK_MANAGEMENT,
				       RZI_LORAWAN_NETWORK_OPS_VERSION,
				       sizeof(struct rzi_lorawan_network_ops));
}

static const struct rzi_lorawan_channel_ops *channel_ops(void)
{
	if ((rzi_lorawan_get_capabilities() & RZI_LORAWAN_CAP_CHANNEL_MANAGEMENT) == 0U) {
		return NULL;
	}
	return rzi_lorawan_feature_ops(RZI_LORAWAN_FEATURE_CHANNEL_MANAGEMENT,
				       RZI_LORAWAN_CHANNEL_OPS_VERSION,
				       sizeof(struct rzi_lorawan_channel_ops));
}

static const struct rzi_lorawan_info_ops *info_ops(void)
{
	return rzi_lorawan_feature_ops(RZI_LORAWAN_FEATURE_INFORMATION,
				       RZI_LORAWAN_INFO_OPS_VERSION,
				       sizeof(struct rzi_lorawan_info_ops));
}

static bool data_rate_valid(enum rzi_lorawan_data_rate data_rate)
{
	return (unsigned int)data_rate <= RZI_LORAWAN_DR_15;
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
	if (!data_rate_valid(data_rate)) {
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
	if (!data_rate_valid(data_rate)) {
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

int rzi_lorawan_get_net_id(uint32_t *net_id)
{
	const struct rzi_lorawan_info_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (net_id == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = info_ops();
	if (ops == NULL || ops->get_net_id == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_net_id(net_id);
}

int rzi_lorawan_get_dev_nonce(uint16_t *dev_nonce)
{
	const struct rzi_lorawan_info_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (dev_nonce == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = info_ops();
	if (ops == NULL || ops->get_dev_nonce == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_dev_nonce(dev_nonce);
}

int rzi_lorawan_set_dev_nonce(uint16_t dev_nonce)
{
	const struct rzi_lorawan_info_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = info_ops();
	if (ops == NULL || ops->set_dev_nonce == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_dev_nonce(dev_nonce);
}

int rzi_lorawan_query_tx_possible(size_t size)
{
	const struct rzi_lorawan_info_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = info_ops();
	if (ops == NULL || ops->query_tx_possible == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->query_tx_possible(size);
}

int rzi_lorawan_get_ping_slot_periodicity(uint8_t *periodicity)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (periodicity == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_ping_slot_periodicity == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_ping_slot_periodicity(periodicity);
}

int rzi_lorawan_set_ping_slot_periodicity(uint8_t periodicity)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (periodicity > 7U) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->set_ping_slot_periodicity == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_ping_slot_periodicity(periodicity);
}

int rzi_lorawan_get_beacon_frequency(uint32_t *frequency_hz)
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
	if (ops == NULL || ops->get_beacon_frequency == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_beacon_frequency(frequency_hz);
}

int rzi_lorawan_get_beacon_time(uint32_t *gps_time)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (gps_time == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_beacon_time == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_beacon_time(gps_time);
}

int rzi_lorawan_get_beacon_data_rate(enum rzi_lorawan_data_rate *data_rate)
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
	if (ops == NULL || ops->get_beacon_data_rate == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_beacon_data_rate(data_rate);
}

int rzi_lorawan_get_beacon_gateway(struct rzi_lorawan_beacon_gateway *gateway)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (gateway == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_beacon_gateway == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_beacon_gateway(gateway);
}

int rzi_lorawan_get_class_b_state(enum rzi_lorawan_class_b_state *state)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (state == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = network_ops();
	if (ops == NULL || ops->get_class_b_state == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_class_b_state(state);
}

int rzi_lorawan_stop_class_b(void)
{
	const struct rzi_lorawan_network_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = network_ops();
	if (ops == NULL || ops->stop_class_b == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->stop_class_b();
}
