/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN certification-mode public implementation.
 */

#include <errno.h>

#include <rzi/lorawan/certification.h>
#include <rzi/lorawan/lorawan.h>

#include "../../backend/lorawan_feature.h"
#include "../../core/lorawan_priv.h"
#include "lorawan_certification.h"

static const struct rzi_lorawan_certification_ops *cert_ops(void)
{
	if ((rzi_lorawan_get_capabilities() & RZI_LORAWAN_CAP_CERTIFICATION) == 0U) {
		return NULL;
	}
	return rzi_lorawan_feature_ops(RZI_LORAWAN_FEATURE_CERTIFICATION,
				       RZI_LORAWAN_CERTIFICATION_OPS_VERSION,
				       sizeof(struct rzi_lorawan_certification_ops));
}

int rzi_lorawan_get_certification_mode(bool *enabled)
{
	const struct rzi_lorawan_certification_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (enabled == NULL) {
		return -EINVAL;
	}
	ops = cert_ops();
	if (ops == NULL || ops->get_mode == NULL) {
		return -ENOTSUP;
	}
	return ops->get_mode(enabled);
}

int rzi_lorawan_set_certification_mode(bool enabled)
{
	const struct rzi_lorawan_certification_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = cert_ops();
	if (ops == NULL || ops->set_mode == NULL) {
		return -ENOTSUP;
	}
	return ops->set_mode(enabled);
}

int rzi_lorawan_get_certification_port_enabled(bool *enabled)
{
	const struct rzi_lorawan_certification_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (enabled == NULL) {
		return -EINVAL;
	}
	ops = cert_ops();
	if (ops == NULL || ops->get_port_enabled == NULL) {
		return -ENOTSUP;
	}
	return ops->get_port_enabled(enabled);
}

int rzi_lorawan_set_certification_port_enabled(bool enabled)
{
	const struct rzi_lorawan_certification_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = cert_ops();
	if (ops == NULL || ops->set_port_enabled == NULL) {
		return -ENOTSUP;
	}
	return ops->set_port_enabled(enabled);
}
