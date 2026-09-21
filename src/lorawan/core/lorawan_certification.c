/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN certification-mode public implementation.
 */

#include <errno.h>

#include <rzi/lorawan/certification.h>
#include <rzi/lorawan/lorawan.h>

#include "../backend/lorawan_backend.h"
#include "lorawan_priv.h"

static const struct rzi_lorawan_certification_ops *cert_ops(void)
{
	return rzi_lorawan_backend.certification;
}

int rzi_lorawan_get_certification_mode(bool *enabled)
{
	const struct rzi_lorawan_certification_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (enabled == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = cert_ops();
	if (ops == NULL || ops->get_mode == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
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
		return -RZI_ERR_NOT_SUPPORTED;
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
		return -RZI_ERR_INVALID;
	}
	ops = cert_ops();
	if (ops == NULL || ops->get_port_enabled == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
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
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_port_enabled(enabled);
}
