/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN Class B public implementation.
 */

#include <errno.h>

#include <rzi/lorawan/lorawan.h>

#include "../backend/lorawan_backend.h"
#include "../lorawan_priv.h"

static const struct rzi_lorawan_class_b_ops *class_b_ops(void)
{
	return rzi_lorawan_backend.class_b;
}

int rzi_lorawan_get_ping_slot_periodicity(uint8_t *periodicity)
{
	const struct rzi_lorawan_class_b_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (periodicity == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = class_b_ops();
	if (ops == NULL || ops->get_ping_slot_periodicity == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_ping_slot_periodicity(periodicity);
}

int rzi_lorawan_set_ping_slot_periodicity(uint8_t periodicity)
{
	const struct rzi_lorawan_class_b_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (periodicity > 7U) {
		return -RZI_ERR_INVALID;
	}
	ops = class_b_ops();
	if (ops == NULL || ops->set_ping_slot_periodicity == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_ping_slot_periodicity(periodicity);
}

int rzi_lorawan_get_beacon_frequency(uint32_t *frequency_hz)
{
	const struct rzi_lorawan_class_b_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (frequency_hz == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = class_b_ops();
	if (ops == NULL || ops->get_beacon_frequency == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_beacon_frequency(frequency_hz);
}

int rzi_lorawan_get_beacon_time(uint32_t *gps_time)
{
	const struct rzi_lorawan_class_b_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (gps_time == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = class_b_ops();
	if (ops == NULL || ops->get_beacon_time == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_beacon_time(gps_time);
}

int rzi_lorawan_get_beacon_data_rate(enum rzi_lorawan_data_rate *data_rate)
{
	const struct rzi_lorawan_class_b_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (data_rate == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = class_b_ops();
	if (ops == NULL || ops->get_beacon_data_rate == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_beacon_data_rate(data_rate);
}

int rzi_lorawan_get_beacon_gateway(struct rzi_lorawan_beacon_gateway *gateway)
{
	const struct rzi_lorawan_class_b_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (gateway == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = class_b_ops();
	if (ops == NULL || ops->get_beacon_gateway == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_beacon_gateway(gateway);
}

int rzi_lorawan_get_class_b_state(enum rzi_lorawan_class_b_state *state)
{
	const struct rzi_lorawan_class_b_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (state == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = class_b_ops();
	if (ops == NULL || ops->get_state == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_state(state);
}

int rzi_lorawan_stop_class_b(void)
{
	const struct rzi_lorawan_class_b_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = class_b_ops();
	if (ops == NULL || ops->stop == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	rc = ops->stop();
	if (rc == 0) {
		rzi_lorawan_note_class(RZI_LORAWAN_CLASS_A);
	}
	return rc;
}
