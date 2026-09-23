/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief USP/LBM multicast-session operations.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/sys/util.h>

#include "lorawan_backend_usp_priv.h"

#define MULTICAST_GROUPS 4U

static struct rzi_lorawan_multicast_session multicast[MULTICAST_GROUPS];
static bool multicast_used[MULTICAST_GROUPS];

static int8_t allocate_group(int8_t requested)
{
	if (requested >= 0) {
		return requested < (int8_t)MULTICAST_GROUPS && !multicast_used[requested]
			       ? requested
			       : -1;
	}
	for (int8_t i = 0; i < (int8_t)MULTICAST_GROUPS; ++i) {
		if (!multicast_used[i]) {
			return i;
		}
	}
	return -1;
}

static int usp_multicast_add(const struct rzi_lorawan_multicast_session *session)
{
	int8_t group;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	group = allocate_group(session->group_id);
	if (group < 0) {
		return rzi_lorawan_usp_finish(-RZI_ERR_NO_RESOURCE);
	}
	rc = rzi_lorawan_usp_result(smtc_modem_multicast_set_grp_config(
		RZI_LORAWAN_USP_STACK_ID, (smtc_modem_mc_grp_id_t)group, session->dev_addr,
		session->network_session_key, session->application_session_key));
	if (rc == 0 && session->device_class == RZI_LORAWAN_CLASS_C) {
		rc = rzi_lorawan_usp_result(smtc_modem_multicast_class_c_start_session(
			RZI_LORAWAN_USP_STACK_ID, (smtc_modem_mc_grp_id_t)group,
			session->frequency_hz, (uint8_t)session->data_rate));
	} else if (rc == 0) {
		smtc_modem_class_b_ping_slot_periodicity_t periodicity =
			(smtc_modem_class_b_ping_slot_periodicity_t)MIN(session->periodicity, 7U);

		rc = rzi_lorawan_usp_result(smtc_modem_multicast_class_b_start_session(
			RZI_LORAWAN_USP_STACK_ID, (smtc_modem_mc_grp_id_t)group,
			session->frequency_hz, (uint8_t)session->data_rate, periodicity));
	}
	if (rc == 0) {
		multicast[group] = *session;
		multicast[group].group_id = group;
		memset(multicast[group].application_session_key, 0,
		       sizeof(multicast[group].application_session_key));
		memset(multicast[group].network_session_key, 0,
		       sizeof(multicast[group].network_session_key));
		multicast_used[group] = true;
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_multicast_remove(uint32_t dev_addr)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	for (size_t i = 0; i < MULTICAST_GROUPS; ++i) {
		if (!multicast_used[i] || multicast[i].dev_addr != dev_addr) {
			continue;
		}
		if (multicast[i].device_class == RZI_LORAWAN_CLASS_C) {
			rc = rzi_lorawan_usp_result(smtc_modem_multicast_class_c_stop_session(
				RZI_LORAWAN_USP_STACK_ID, (smtc_modem_mc_grp_id_t)i));
		} else {
			rc = rzi_lorawan_usp_result(smtc_modem_multicast_class_b_stop_session(
				RZI_LORAWAN_USP_STACK_ID, (smtc_modem_mc_grp_id_t)i));
		}
		if (rc == 0) {
			multicast_used[i] = false;
		}
		return rzi_lorawan_usp_finish(rc);
	}
	return rzi_lorawan_usp_finish(-RZI_ERR_NOT_FOUND);
}

static int usp_multicast_count(size_t *count)
{
	size_t n = 0;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	for (size_t i = 0; i < MULTICAST_GROUPS; ++i) {
		if (multicast_used[i]) {
			n++;
		}
	}
	*count = n;
	return rzi_lorawan_usp_finish(0);
}

static int usp_multicast_get(size_t index, struct rzi_lorawan_multicast_session *session)
{
	size_t n = 0;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	for (size_t i = 0; i < MULTICAST_GROUPS; ++i) {
		if (!multicast_used[i]) {
			continue;
		}
		if (n == index) {
			*session = multicast[i];
			return rzi_lorawan_usp_finish(0);
		}
		n++;
	}
	return rzi_lorawan_usp_finish(-RZI_ERR_INVALID);
}

static int usp_multicast_clear(void)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(
		smtc_modem_multicast_class_c_stop_all_sessions(RZI_LORAWAN_USP_STACK_ID));
	if (rc == 0) {
		rc = rzi_lorawan_usp_result(
			smtc_modem_multicast_class_b_stop_all_sessions(RZI_LORAWAN_USP_STACK_ID));
	}
	if (rc == 0) {
		memset(multicast_used, 0, sizeof(multicast_used));
	}
	return rzi_lorawan_usp_finish(rc);
}

const struct rzi_lorawan_multicast_ops usp_multicast_ops = {
	.add = usp_multicast_add,
	.remove = usp_multicast_remove,
	.get_count = usp_multicast_count,
	.get = usp_multicast_get,
	.clear = usp_multicast_clear,
};
