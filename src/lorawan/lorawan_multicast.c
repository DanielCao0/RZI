/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN multicast-session public implementation.
 */

#include <errno.h>

#include <rzi/lorawan/multicast.h>

#include "backend/lorawan_backend.h"
#include "lorawan_priv.h"

static const struct rzi_lorawan_multicast_ops *multicast_ops(void)
{
	return rzi_lorawan_backend.multicast;
}

int rzi_lorawan_add_multicast_session(const struct rzi_lorawan_multicast_session *session)
{
	const struct rzi_lorawan_multicast_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (session == NULL ||
	    (session->device_class != RZI_LORAWAN_CLASS_B &&
	     session->device_class != RZI_LORAWAN_CLASS_C) ||
	    !rzi_lorawan_rx_data_rate_valid(session->data_rate) || session->frequency_hz == 0U ||
	    session->group_id < -1 || session->group_id > 3) {
		return -RZI_ERR_INVALID;
	}
	ops = multicast_ops();
	if (ops == NULL || ops->add == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->add(session);
}

int rzi_lorawan_remove_multicast_session(uint32_t dev_addr)
{
	const struct rzi_lorawan_multicast_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = multicast_ops();
	if (ops == NULL || ops->remove == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->remove(dev_addr);
}

int rzi_lorawan_get_multicast_count(size_t *count)
{
	const struct rzi_lorawan_multicast_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (count == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = multicast_ops();
	if (ops == NULL || ops->get_count == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_count(count);
}

int rzi_lorawan_get_multicast_session(size_t index, struct rzi_lorawan_multicast_session *session)
{
	const struct rzi_lorawan_multicast_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (session == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = multicast_ops();
	if (ops == NULL || ops->get == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get(index, session);
}

int rzi_lorawan_clear_multicast_sessions(void)
{
	const struct rzi_lorawan_multicast_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = multicast_ops();
	if (ops == NULL || ops->clear == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->clear();
}
