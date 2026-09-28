/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief USP/LBM OTAA session identity readback.
 */

#include <stdint.h>

#include <smtc_secure_element.h>

#include "lorawan_backend_usp_priv.h"

extern uint32_t lorawan_api_devaddr_get(uint8_t stack_id);

static int usp_get_dev_addr(uint32_t *dev_addr)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	*dev_addr = lorawan_api_devaddr_get(RZI_LORAWAN_USP_STACK_ID);
	return rzi_lorawan_usp_finish(0);
}

static int usp_copy_session_key(smtc_se_key_identifier_t key_id, uint8_t *key)
{
	smtc_se_return_code_t se_rc;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	se_rc = smtc_secure_element_get_key(key_id, key, RZI_LORAWAN_USP_STACK_ID);
	if (se_rc == SMTC_SE_RC_SUCCESS) {
		return rzi_lorawan_usp_finish(0);
	}
	if (se_rc == SMTC_SE_RC_ERROR) {
		return rzi_lorawan_usp_finish(-RZI_ERR_NOT_SUPPORTED);
	}
	return rzi_lorawan_usp_finish(-RZI_ERR_IO);
}

static int usp_get_nwk_skey(uint8_t *key)
{
	return usp_copy_session_key(SMTC_SE_NWK_S_ENC_KEY, key);
}

static int usp_get_app_skey(uint8_t *key)
{
	return usp_copy_session_key(SMTC_SE_APP_S_KEY, key);
}

const struct rzi_lorawan_session_ops usp_session_ops = {
	.get_dev_addr = usp_get_dev_addr,
	.get_nwk_skey = usp_get_nwk_skey,
	.get_app_skey = usp_get_app_skey,
};
