/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief USP/LBM MAC-request operations.
 */

#include "lorawan_backend_usp_priv.h"

static int usp_link_check_request(void)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(smtc_modem_trig_lorawan_mac_request(
		RZI_LORAWAN_USP_STACK_ID, SMTC_MODEM_LORAWAN_MAC_REQ_LINK_CHECK)));
}

static int usp_device_time_request(void)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(smtc_modem_trig_lorawan_mac_request(
		RZI_LORAWAN_USP_STACK_ID, SMTC_MODEM_LORAWAN_MAC_REQ_DEVICE_TIME)));
}

static int usp_get_network_time(struct rzi_lorawan_network_time *time)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(smtc_modem_get_lorawan_mac_time(
		RZI_LORAWAN_USP_STACK_ID, &time->gps_seconds, &time->gps_subseconds));
	return rzi_lorawan_usp_finish(rc);
}

const struct rzi_lorawan_mac_ops usp_mac_ops = {
	.request_link_check = usp_link_check_request,
	.request_device_time = usp_device_time_request,
	.get_network_time = usp_get_network_time,
};
