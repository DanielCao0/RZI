/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN session-information public implementation.
 */

#include <errno.h>

#include <rzi/lorawan/lorawan.h>

#include "../backend/lorawan_backend.h"
#include "../lorawan_priv.h"

static const struct rzi_lorawan_session_ops *session_ops(void)
{
	return rzi_lorawan_backend.session;
}

int rzi_lorawan_get_net_id(uint32_t *net_id)
{
	const struct rzi_lorawan_session_ops *ops;
	bool joined = false;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0 || net_id == NULL) {
		return rc != 0 ? rc : -RZI_ERR_INVALID;
	}
	ops = session_ops();
	if (rzi_lorawan_is_joined(&joined) == 0 && joined && ops != NULL &&
	    ops->get_net_id != NULL) {
		rc = ops->get_net_id(net_id);
		if (rc == 0) {
			return 0;
		}
	}
	return rzi_lorawan_config_get_net_id(net_id);
}

int rzi_lorawan_get_dev_nonce(uint16_t *dev_nonce)
{
	const struct rzi_lorawan_session_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	if (dev_nonce == NULL) {
		return -RZI_ERR_INVALID;
	}
	ops = session_ops();
	if (ops == NULL || ops->get_dev_nonce == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_dev_nonce(dev_nonce);
}

int rzi_lorawan_set_dev_nonce(uint16_t dev_nonce)
{
	const struct rzi_lorawan_session_ops *ops;
	int rc = rzi_lorawan_check_started();

	if (rc != 0) {
		return rc;
	}
	ops = session_ops();
	if (ops == NULL || ops->set_dev_nonce == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->set_dev_nonce(dev_nonce);
}
