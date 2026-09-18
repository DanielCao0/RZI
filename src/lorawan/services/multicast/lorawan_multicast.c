/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN multicast-session public implementation.
 */

#include <errno.h>

#include <rzi/lorawan/multicast.h>

#include "../../backend/lorawan_feature.h"
#include "../../core/lorawan_priv.h"
#include "lorawan_multicast.h"

static const struct rzi_lorawan_multicast_ops *multicast_ops(void)
{
	if ((rzi_lorawan_get_capabilities() & RZI_LORAWAN_CAP_MULTICAST) == 0U) {
		return NULL;
	}
	return rzi_lorawan_feature_ops(RZI_LORAWAN_FEATURE_MULTICAST,
				       RZI_LORAWAN_MULTICAST_OPS_VERSION,
				       sizeof(struct rzi_lorawan_multicast_ops));
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
	    (unsigned int)session->data_rate > RZI_LORAWAN_DR_15 || session->frequency_hz == 0U ||
	    session->group_id < -1 || session->group_id > 3) {
		return -EINVAL;
	}
	ops = multicast_ops();
	if (ops == NULL || ops->add == NULL) {
		return -ENOTSUP;
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
		return -ENOTSUP;
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
		return -EINVAL;
	}
	ops = multicast_ops();
	if (ops == NULL || ops->get_count == NULL) {
		return -ENOTSUP;
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
		return -EINVAL;
	}
	ops = multicast_ops();
	if (ops == NULL || ops->get == NULL) {
		return -ENOTSUP;
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
		return -ENOTSUP;
	}
	return ops->clear();
}
