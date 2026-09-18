/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN channel-scan public implementation.
 */

#include <errno.h>

#include <rzi/lorawan/channel_scan.h>
#include <rzi/lorawan/lorawan.h>

#include "../../backend/lorawan_feature.h"
#include "../../core/lorawan_priv.h"
#include "lorawan_channel_scan.h"

static const struct rzi_lorawan_channel_scan_ops *scan_ops(void)
{
	if ((rzi_lorawan_get_capabilities() & RZI_LORAWAN_CAP_CHANNEL_SCAN) == 0U) {
		return NULL;
	}
	return rzi_lorawan_feature_ops(RZI_LORAWAN_FEATURE_CHANNEL_SCAN,
				       RZI_LORAWAN_CHANNEL_SCAN_OPS_VERSION,
				       sizeof(struct rzi_lorawan_channel_scan_ops));
}

int rzi_lorawan_get_channel_rssi_count(size_t *count)
{
	const struct rzi_lorawan_channel_scan_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (count == NULL) {
		return -EINVAL;
	}
	ops = scan_ops();
	if (ops == NULL || ops->get_count == NULL) {
		return -ENOTSUP;
	}
	return ops->get_count(count);
}

int rzi_lorawan_get_channel_rssi(size_t index, struct rzi_lorawan_channel_rssi *rssi)
{
	const struct rzi_lorawan_channel_scan_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (rssi == NULL) {
		return -EINVAL;
	}
	ops = scan_ops();
	if (ops == NULL || ops->get == NULL) {
		return -ENOTSUP;
	}
	return ops->get(index, rssi);
}
