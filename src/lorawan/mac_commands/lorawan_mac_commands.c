/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief DeviceTimeReq and LinkCheckReq public implementation.
 */

#include <errno.h>

#include <rzi/lorawan/mac_commands.h>

#include "../backend/lorawan_feature.h"
#include "../core/lorawan_priv.h"
#include "lorawan_mac_commands.h"

static enum rzi_lorawan_link_check_mode link_check_mode = RZI_LORAWAN_LINK_CHECK_DISABLED;
static bool device_time_enabled;

static const struct rzi_lorawan_link_check_ops *link_check_ops(void)
{
	if ((rzi_lorawan_get_capabilities() & RZI_LORAWAN_CAP_LINK_CHECK) == 0U) {
		return NULL;
	}
	return rzi_lorawan_feature_ops(RZI_LORAWAN_FEATURE_LINK_CHECK,
				       RZI_LORAWAN_LINK_CHECK_OPS_VERSION,
				       sizeof(struct rzi_lorawan_link_check_ops));
}

static const struct rzi_lorawan_device_time_ops *device_time_ops(void)
{
	if ((rzi_lorawan_get_capabilities() & RZI_LORAWAN_CAP_DEVICE_TIME) == 0U) {
		return NULL;
	}
	return rzi_lorawan_feature_ops(RZI_LORAWAN_FEATURE_DEVICE_TIME,
				       RZI_LORAWAN_DEVICE_TIME_OPS_VERSION,
				       sizeof(struct rzi_lorawan_device_time_ops));
}

static int trig_link_check(void)
{
	const struct rzi_lorawan_link_check_ops *ops = link_check_ops();

	if (ops == NULL || ops->request == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->request();
}

static int trig_device_time(void)
{
	const struct rzi_lorawan_device_time_ops *ops = device_time_ops();

	if (ops == NULL || ops->request == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->request();
}

int rzi_lorawan_get_link_check_mode(enum rzi_lorawan_link_check_mode *mode)
{
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (mode == NULL) {
		return -RZI_ERR_INVALID;
	}
	if (link_check_ops() == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	*mode = link_check_mode;
	return 0;
}

int rzi_lorawan_request_link_check(enum rzi_lorawan_link_check_mode mode)
{
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if ((unsigned int)mode > RZI_LORAWAN_LINK_CHECK_EVERY_UPLINK) {
		return -RZI_ERR_INVALID;
	}
	if (link_check_ops() == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	link_check_mode = mode;
	if (mode == RZI_LORAWAN_LINK_CHECK_DISABLED) {
		return 0;
	}
	rc = trig_link_check();
	if (rc == 0 && mode == RZI_LORAWAN_LINK_CHECK_ONCE) {
		link_check_mode = RZI_LORAWAN_LINK_CHECK_DISABLED;
	}
	return rc;
}

int rzi_lorawan_get_device_time_enabled(bool *enabled)
{
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (enabled == NULL) {
		return -RZI_ERR_INVALID;
	}
	if (device_time_ops() == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	*enabled = device_time_enabled;
	return 0;
}

int rzi_lorawan_request_device_time(bool enabled)
{
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (device_time_ops() == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	device_time_enabled = enabled;
	if (!enabled) {
		return 0;
	}
	return trig_device_time();
}

int rzi_lorawan_get_network_time(struct rzi_lorawan_network_time *time)
{
	const struct rzi_lorawan_device_time_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (time == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = device_time_ops();
	if (ops == NULL || ops->get_network_time == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_network_time(time);
}

void rzi_lorawan_mac_commands_on_uplink(void)
{
	if (link_check_mode == RZI_LORAWAN_LINK_CHECK_EVERY_UPLINK) {
		(void)trig_link_check();
	}
	if (device_time_enabled) {
		(void)trig_device_time();
	}
}
