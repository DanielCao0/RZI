/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief USP/LBM certification-mode operations.
 */

#include "lorawan_backend_usp_priv.h"

static bool cert_port_enabled = true;

static int usp_get_cert_mode(bool *enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(
		smtc_modem_get_certification_mode(RZI_LORAWAN_USP_STACK_ID, enabled)));
}

static int usp_set_cert_mode(bool enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(
		smtc_modem_set_certification_mode(RZI_LORAWAN_USP_STACK_ID, enabled)));
}

static int usp_get_cert_port(bool *enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	*enabled = cert_port_enabled;
	return rzi_lorawan_usp_finish(0);
}

static int usp_set_cert_port(bool enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	cert_port_enabled = enabled;
	return rzi_lorawan_usp_finish(0);
}

const struct rzi_lorawan_certification_ops usp_cert_ops = {
	.get_mode = usp_get_cert_mode,
	.set_mode = usp_set_cert_mode,
	.get_port_enabled = usp_get_cert_port,
	.set_port_enabled = usp_set_cert_port,
};
