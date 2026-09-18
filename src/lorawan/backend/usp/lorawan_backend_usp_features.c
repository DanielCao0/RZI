/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Optional USP/LBM feature operations for the RZI LoRaWAN backend.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "../../mac_commands/lorawan_mac_commands.h"
#include "../../services/certification/lorawan_certification.h"
#include "../../services/multicast/lorawan_multicast.h"
#include "lorawan_backend_usp_priv.h"

#ifdef CONFIG_RZI_LORAWAN_FUOTA
#include "../../services/fuota/lorawan_fuota.h"
#endif

#define LBT_DEFAULT_DURATION_MS 5U
#define LBT_DEFAULT_THRESHOLD   (-80)
#define LBT_DEFAULT_BW_HZ       200000U
#define MULTICAST_GROUPS        4U

static enum rzi_lorawan_data_rate cached_data_rate = RZI_LORAWAN_DR_0;
static bool cert_port_enabled = true;
static struct rzi_lorawan_multicast_session multicast[MULTICAST_GROUPS];
static bool multicast_used[MULTICAST_GROUPS];

static int apply_custom_adr(enum rzi_lorawan_data_rate data_rate)
{
	uint8_t custom[SMTC_MODEM_CUSTOM_ADR_DATA_LENGTH] = {0};

	custom[data_rate] = 100U;
	return rzi_lorawan_usp_result(smtc_modem_adr_set_profile(
		RZI_LORAWAN_USP_STACK_ID, SMTC_MODEM_ADR_PROFILE_CUSTOM, custom));
}

static int usp_get_class(enum rzi_lorawan_class *device_class)
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
			rc = -EINVAL;
			break;
		}
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_get_adr(bool *enabled)
{
	smtc_modem_adr_profile_t profile;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(smtc_modem_adr_get_profile(RZI_LORAWAN_USP_STACK_ID, &profile));
	if (rc == 0) {
		*enabled = profile == SMTC_MODEM_ADR_PROFILE_NETWORK_CONTROLLED;
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_set_adr(bool enabled)
{
	uint8_t custom[SMTC_MODEM_CUSTOM_ADR_DATA_LENGTH] = {0};
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	if (enabled) {
		rc = rzi_lorawan_usp_result(smtc_modem_adr_set_profile(
			RZI_LORAWAN_USP_STACK_ID, SMTC_MODEM_ADR_PROFILE_NETWORK_CONTROLLED,
			custom));
	} else {
		rc = apply_custom_adr(cached_data_rate);
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_get_data_rate(enum rzi_lorawan_data_rate *data_rate)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	*data_rate = cached_data_rate;
	return rzi_lorawan_usp_finish(0);
}

static int usp_set_data_rate(enum rzi_lorawan_data_rate data_rate)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = apply_custom_adr(data_rate);
	if (rc == 0) {
		cached_data_rate = data_rate;
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_get_public_network(bool *enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(
		smtc_modem_get_network_type(RZI_LORAWAN_USP_STACK_ID, enabled)));
}

static int usp_set_public_network(bool enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(
		smtc_modem_set_network_type(RZI_LORAWAN_USP_STACK_ID, enabled)));
}

static int usp_get_lbt(bool *enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(
		smtc_modem_lbt_get_state(RZI_LORAWAN_USP_STACK_ID, enabled)));
}

static int usp_set_lbt(bool enabled)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	if (enabled) {
		rc = rzi_lorawan_usp_result(smtc_modem_lbt_set_parameters(
			RZI_LORAWAN_USP_STACK_ID, LBT_DEFAULT_DURATION_MS, LBT_DEFAULT_THRESHOLD,
			LBT_DEFAULT_BW_HZ));
		if (rc != 0) {
			return rzi_lorawan_usp_finish(rc);
		}
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(
		smtc_modem_lbt_set_state(RZI_LORAWAN_USP_STACK_ID, enabled)));
}

static int usp_get_lbt_rssi(int16_t *rssi_dbm)
{
	uint32_t duration_ms;
	uint32_t bw_hz;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(smtc_modem_lbt_get_parameters(
		RZI_LORAWAN_USP_STACK_ID, &duration_ms, rssi_dbm, &bw_hz)));
}

static int usp_set_lbt_rssi(int16_t rssi_dbm)
{
	uint32_t duration_ms;
	int16_t current;
	uint32_t bw_hz;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(smtc_modem_lbt_get_parameters(RZI_LORAWAN_USP_STACK_ID,
								  &duration_ms, &current, &bw_hz));
	if (rc != 0) {
		duration_ms = LBT_DEFAULT_DURATION_MS;
		bw_hz = LBT_DEFAULT_BW_HZ;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(smtc_modem_lbt_set_parameters(
		RZI_LORAWAN_USP_STACK_ID, duration_ms, rssi_dbm, bw_hz)));
}

static int usp_get_lbt_scan_time(uint32_t *time_ms)
{
	int16_t threshold;
	uint32_t bw_hz;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(smtc_modem_lbt_get_parameters(
		RZI_LORAWAN_USP_STACK_ID, time_ms, &threshold, &bw_hz)));
}

static int usp_set_lbt_scan_time(uint32_t time_ms)
{
	int16_t threshold;
	uint32_t duration_ms;
	uint32_t bw_hz;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(smtc_modem_lbt_get_parameters(
		RZI_LORAWAN_USP_STACK_ID, &duration_ms, &threshold, &bw_hz));
	if (rc != 0) {
		threshold = LBT_DEFAULT_THRESHOLD;
		bw_hz = LBT_DEFAULT_BW_HZ;
	}
	return rzi_lorawan_usp_finish(rzi_lorawan_usp_result(smtc_modem_lbt_set_parameters(
		RZI_LORAWAN_USP_STACK_ID, time_ms, threshold, bw_hz)));
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
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(
		smtc_modem_set_class(RZI_LORAWAN_USP_STACK_ID, SMTC_MODEM_CLASS_A));
	if (rc == 0) {
		rzi_lorawan_usp_set_class_b_state(RZI_LORAWAN_CLASS_B_IDLE);
	}
	return rzi_lorawan_usp_finish(rc);
}

static const struct rzi_lorawan_network_ops usp_network_ops = {
	.get_class = usp_get_class,
	.get_adr = usp_get_adr,
	.set_adr = usp_set_adr,
	.get_data_rate = usp_get_data_rate,
	.set_data_rate = usp_set_data_rate,
	.get_public_network = usp_get_public_network,
	.set_public_network = usp_set_public_network,
	.get_lbt = usp_get_lbt,
	.set_lbt = usp_set_lbt,
	.get_lbt_rssi = usp_get_lbt_rssi,
	.set_lbt_rssi = usp_set_lbt_rssi,
	.get_lbt_scan_time = usp_get_lbt_scan_time,
	.set_lbt_scan_time = usp_set_lbt_scan_time,
	.get_ping_slot_periodicity = usp_get_ping_slot,
	.set_ping_slot_periodicity = usp_set_ping_slot,
	.get_class_b_state = usp_get_class_b_state,
	.stop_class_b = usp_stop_class_b,
};

static int usp_query_tx_possible(size_t size)
{
	uint8_t max_payload = 0;
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	rc = rzi_lorawan_usp_result(
		smtc_modem_get_next_tx_max_payload(RZI_LORAWAN_USP_STACK_ID, &max_payload));
	if (rc == 0 && size > max_payload) {
		rc = -EMSGSIZE;
	}
	return rzi_lorawan_usp_finish(rc);
}

static int usp_info_is_busy(bool *busy)
{
	int rc = rzi_lorawan_usp_enter();

	if (rc != 0) {
		return rc;
	}
	*busy = rzi_lorawan_usp_tx_pending();
	return rzi_lorawan_usp_finish(0);
}

static const struct rzi_lorawan_info_ops usp_info_ops = {
	.query_tx_possible = usp_query_tx_possible,
	.is_busy = usp_info_is_busy,
};

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

static const struct rzi_lorawan_link_check_ops usp_link_check_ops = {
	.request = usp_link_check_request,
};

static const struct rzi_lorawan_device_time_ops usp_device_time_ops = {
	.request = usp_device_time_request,
	.get_network_time = usp_get_network_time,
};

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
		return rzi_lorawan_usp_finish(-ENOMEM);
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
	return rzi_lorawan_usp_finish(-ENOENT);
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
	return rzi_lorawan_usp_finish(-EINVAL);
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

static const struct rzi_lorawan_multicast_ops usp_multicast_ops = {
	.add = usp_multicast_add,
	.remove = usp_multicast_remove,
	.get_count = usp_multicast_count,
	.get = usp_multicast_get,
	.clear = usp_multicast_clear,
};

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

static const struct rzi_lorawan_certification_ops usp_cert_ops = {
	.get_mode = usp_get_cert_mode,
	.set_mode = usp_set_cert_mode,
	.get_port_enabled = usp_get_cert_port,
	.set_port_enabled = usp_set_cert_port,
};

static const struct rzi_lorawan_backend_extension usp_network_ext = {
	.size = sizeof(usp_network_ops),
	.version = RZI_LORAWAN_NETWORK_OPS_VERSION,
	.api = &usp_network_ops,
};

static const struct rzi_lorawan_backend_extension usp_info_ext = {
	.size = sizeof(usp_info_ops),
	.version = RZI_LORAWAN_INFO_OPS_VERSION,
	.api = &usp_info_ops,
};

static const struct rzi_lorawan_backend_extension usp_link_check_ext = {
	.size = sizeof(usp_link_check_ops),
	.version = RZI_LORAWAN_LINK_CHECK_OPS_VERSION,
	.api = &usp_link_check_ops,
};

static const struct rzi_lorawan_backend_extension usp_device_time_ext = {
	.size = sizeof(usp_device_time_ops),
	.version = RZI_LORAWAN_DEVICE_TIME_OPS_VERSION,
	.api = &usp_device_time_ops,
};

static const struct rzi_lorawan_backend_extension usp_multicast_ext = {
	.size = sizeof(usp_multicast_ops),
	.version = RZI_LORAWAN_MULTICAST_OPS_VERSION,
	.api = &usp_multicast_ops,
};

static const struct rzi_lorawan_backend_extension usp_cert_ext = {
	.size = sizeof(usp_cert_ops),
	.version = RZI_LORAWAN_CERTIFICATION_OPS_VERSION,
	.api = &usp_cert_ops,
};

#ifdef CONFIG_RZI_LORAWAN_FUOTA
extern const struct rzi_lorawan_backend_extension rzi_lorawan_usp_fuota_extension;
#endif

const struct rzi_lorawan_backend_extension *
rzi_lorawan_usp_get_extension(enum rzi_lorawan_feature_id feature)
{
	switch (feature) {
	case RZI_LORAWAN_FEATURE_NETWORK_MANAGEMENT:
		return &usp_network_ext;
	case RZI_LORAWAN_FEATURE_INFORMATION:
		return &usp_info_ext;
	case RZI_LORAWAN_FEATURE_LINK_CHECK:
		return &usp_link_check_ext;
	case RZI_LORAWAN_FEATURE_DEVICE_TIME:
		return &usp_device_time_ext;
	case RZI_LORAWAN_FEATURE_MULTICAST:
		return &usp_multicast_ext;
	case RZI_LORAWAN_FEATURE_CERTIFICATION:
		return &usp_cert_ext;
#ifdef CONFIG_RZI_LORAWAN_FUOTA
	case RZI_LORAWAN_FEATURE_FUOTA:
		return &rzi_lorawan_usp_fuota_extension;
#endif
	default:
		return NULL;
	}
}
