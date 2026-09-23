/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief USP/LBM device-class and Class B operations.
 */

#include <errno.h>

#include "lorawan_backend_usp_priv.h"

int usp_get_class(enum rzi_lorawan_class *device_class)
{
	smtc_modem_class_t mapped;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(smtc_modem_get_class(RZI_LORAWAN_USP_STACK_ID, &mapped));
	if (rc == 0) {
		switch (mapped) {
		case SMTC_MODEM_CLASS_A:
			*device_class = RZI_LORAWAN_CLASS_A;
			break;
		case SMTC_MODEM_CLASS_B:
			*device_class = RZI_LORAWAN_CLASS_B;
			break;
		case SMTC_MODEM_CLASS_C:
			*device_class = RZI_LORAWAN_CLASS_C;
			break;
		default:
			rc = -RZI_ERR_INVALID;
			break;
		}
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_get_ping_slot(uint8_t *periodicity)
{
	smtc_modem_class_b_ping_slot_periodicity_t mapped;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(
		smtc_modem_class_b_get_ping_slot_periodicity(RZI_LORAWAN_USP_STACK_ID, &mapped));
	if (rc == 0) {
		*periodicity = (uint8_t)mapped;
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_set_ping_slot(uint8_t periodicity)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(
		rzi_lorawan_usp_result(smtc_modem_class_b_set_ping_slot_periodicity(
			RZI_LORAWAN_USP_STACK_ID,
			(smtc_modem_class_b_ping_slot_periodicity_t)periodicity)));
}

static int usp_get_class_b_state(enum rzi_lorawan_class_b_state *state)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	*state = rzi_lorawan_usp_class_b_state();
	return rzi_lorawan_usp_finish(0);
}

static int usp_stop_class_b(void)
{
	bool notify = false;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(
		smtc_modem_set_class(RZI_LORAWAN_USP_STACK_ID, SMTC_MODEM_CLASS_A));
	if (rc == 0) {
		notify = rzi_lorawan_usp_set_class_b_state(RZI_LORAWAN_CLASS_B_IDLE);
	}
	if (notify) {
		rzi_lorawan_usp_publish_class_b(RZI_LORAWAN_CLASS_B_IDLE);
	}
	return rzi_lorawan_usp_finish(rc);
}

const struct rzi_lorawan_class_b_ops usp_class_b_ops = {
	.get_ping_slot_periodicity = usp_get_ping_slot,
	.set_ping_slot_periodicity = usp_set_ping_slot,
	.get_state = usp_get_class_b_state,
	.stop = usp_stop_class_b,
};
