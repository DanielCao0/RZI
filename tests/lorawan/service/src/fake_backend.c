/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Test backend that follows USP / LoRa Basics Modem semantics: join keeps
 * retrying one attempt at a time until JOINED or leave(), uplinks complete
 * asynchronously, ADR profiles need a session, and only the controls LBM has
 * are provided.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <rzi/lorawan/lorawan.h>
#include <rzi/lorawan/multicast.h>

#include "backend/lorawan_backend.h"

#define FAKE_CAPABILITIES                                                                          \
	(RZI_LORAWAN_CAP_OTAA | RZI_LORAWAN_CAP_CLASS_A | RZI_LORAWAN_CAP_CLASS_B |                \
	 RZI_LORAWAN_CAP_CLASS_C | RZI_LORAWAN_CAP_NETWORK | RZI_LORAWAN_CAP_CHANNEL |             \
	 RZI_LORAWAN_CAP_SESSION | RZI_LORAWAN_CAP_MAC | RZI_LORAWAN_CAP_MULTICAST |               \
	 RZI_LORAWAN_CAP_CERTIFICATION)

#define FAKE_ATTEMPT_DELAY K_MSEC(5)
#define US915_UPLINK_DRS   0x007fU

rzi_lorawan_event_sink_t fake_sink;
int fake_set_class_error;
/* Join attempts that fail before one succeeds; negative fails forever. */
int fake_join_failures;
int fake_join_attempts;
int fake_leave_calls;
/* Keep uplinks outstanding until fake_complete_tx(). */
bool fake_tx_hold;
struct rzi_lorawan_join_config fake_last_join;

static K_MUTEX_DEFINE(lock);
static void join_attempt(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(join_work, join_attempt);

static enum rzi_lorawan_class device_class = RZI_LORAWAN_CLASS_A;
static enum rzi_lorawan_adr_profile adr_profile = RZI_LORAWAN_ADR_NETWORK;
static enum rzi_lorawan_data_rate data_rate = RZI_LORAWAN_DR_0;
static uint8_t nb_trans = 1;
static bool public_network = true;
static bool lbt_enabled;
static int16_t lbt_rssi_dbm = -80;
static uint32_t lbt_scan_time_ms = 5;
static uint8_t ping_slot;
static uint32_t beacon_frequency_hz = 869525000;
static uint32_t beacon_time;
static enum rzi_lorawan_data_rate beacon_data_rate = RZI_LORAWAN_DR_3;
static struct rzi_lorawan_beacon_gateway beacon_gateway;
static enum rzi_lorawan_class_b_state class_b_state = RZI_LORAWAN_CLASS_B_IDLE;
static uint32_t net_id;
static uint16_t dev_nonce = 1;
static struct rzi_lorawan_network_time network_time;
static bool cert_mode;
static bool cert_port = true;
static struct rzi_lorawan_multicast_session multicast[4];
static bool multicast_used[4];
static bool joining;
static bool session_joined;
static bool tx_pending;
static enum rzi_lorawan_tx_status held_status;

static void publish_type(enum rzi_lorawan_backend_event_type type)
{
	const struct rzi_lorawan_backend_event event = {
		.type = type,
		.error = type == RZI_LORAWAN_BACKEND_JOIN_FAILED ? -RZI_ERR_TIMEOUT : 0,
	};

	fake_sink(&event);
}

static void join_attempt(struct k_work *work)
{
	bool failed;

	ARG_UNUSED(work);
	k_mutex_lock(&lock, K_FOREVER);
	if (!joining) {
		k_mutex_unlock(&lock);
		return;
	}
	fake_join_attempts++;
	failed = fake_join_failures != 0;
	if (failed) {
		if (fake_join_failures > 0) {
			fake_join_failures--;
		}
		/* LBM queues the next attempt before reporting this one. */
		k_work_schedule(&join_work, FAKE_ATTEMPT_DELAY);
	} else {
		joining = false;
		session_joined = true;
	}
	k_mutex_unlock(&lock);
	publish_type(failed ? RZI_LORAWAN_BACKEND_JOIN_FAILED : RZI_LORAWAN_BACKEND_JOINED);
}

static int fake_start(enum rzi_lorawan_region region, bool join_backoff_bypass,
		      rzi_lorawan_event_sink_t event_sink)
{
	zassert_equal(region, RZI_LORAWAN_REGION_US_915);
	zassert_true(join_backoff_bypass);
	fake_sink = event_sink;
	session_joined = false;
	publish_type(RZI_LORAWAN_BACKEND_READY);
	return 0;
}

static int fake_join(const struct rzi_lorawan_join_config *config)
{
	int rc = 0;

	if (config->activation != RZI_LORAWAN_ACTIVATION_OTAA) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	k_mutex_lock(&lock, K_FOREVER);
	if (joining || session_joined) {
		/* smtc_modem_set_deveui() is refused while joining or joined. */
		rc = -RZI_ERR_BUSY;
	} else {
		fake_last_join = *config;
		joining = true;
		k_work_schedule(&join_work, FAKE_ATTEMPT_DELAY);
	}
	k_mutex_unlock(&lock);
	return rc;
}

static int fake_leave(void)
{
	k_mutex_lock(&lock, K_FOREVER);
	fake_leave_calls++;
	joining = false;
	session_joined = false;
	(void)k_work_cancel_delayable(&join_work);
	k_mutex_unlock(&lock);
	return 0;
}

void fake_complete_tx(void)
{
	const struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_TX_DONE,
		.tx_status = held_status,
	};

	tx_pending = false;
	fake_sink(&event);
}

static int start_tx(enum rzi_lorawan_message_type type)
{
	if (tx_pending) {
		return -RZI_ERR_BUSY;
	}
	tx_pending = true;
	held_status = type == RZI_LORAWAN_MSG_CONFIRMED ? RZI_LORAWAN_EVENT_TXDONE_CONFIRMED
							: RZI_LORAWAN_EVENT_TXDONE_SENT;
	if (!fake_tx_hold) {
		fake_complete_tx();
	}
	return 0;
}

static int fake_send(uint8_t port, const uint8_t *data, size_t size,
		     enum rzi_lorawan_message_type type)
{
	zassert_equal(port, 10);
	zassert_equal(size, 2);
	zassert_equal(data[0], 0x12);
	return start_tx(type);
}

static int fake_send_empty(bool with_port, uint8_t port, enum rzi_lorawan_message_type type)
{
	zassert_true(!with_port || port == 20);
	return start_tx(type);
}

static int fake_set_class(enum rzi_lorawan_class requested)
{
	enum rzi_lorawan_class_b_state next = requested == RZI_LORAWAN_CLASS_B
						      ? RZI_LORAWAN_CLASS_B_ACQUIRING_BEACON
						      : RZI_LORAWAN_CLASS_B_IDLE;

	if (fake_set_class_error != 0) {
		return fake_set_class_error;
	}
	device_class = requested;
	if (class_b_state != next) {
		const struct rzi_lorawan_backend_event event = {
			.type = RZI_LORAWAN_BACKEND_CLASS_B,
			.class_b = next,
		};

		class_b_state = next;
		fake_sink(&event);
	}
	return 0;
}

static int fake_is_joined(bool *joined)
{
	*joined = session_joined;
	return 0;
}

static int fake_get_class(enum rzi_lorawan_class *value)
{
	*value = device_class;
	return 0;
}

static int require_joined(void)
{
	return session_joined ? 0 : -RZI_ERR_NOT_JOINED;
}

static int fake_get_adr(bool *enabled)
{
	*enabled = adr_profile == RZI_LORAWAN_ADR_NETWORK;
	return 0;
}

static int fake_set_adr_profile(enum rzi_lorawan_adr_profile profile, const uint8_t *distribution)
{
	int rc = require_joined();

	if (rc != 0) {
		return rc;
	}
	if (profile == RZI_LORAWAN_ADR_CUSTOM) {
		data_rate = (enum rzi_lorawan_data_rate)distribution[0];
	}
	adr_profile = profile;
	return 0;
}

static int fake_set_adr(bool enabled)
{
	uint8_t pinned[RZI_LORAWAN_DR_DISTRIBUTION_LENGTH];

	memset(pinned, data_rate, sizeof(pinned));
	return fake_set_adr_profile(enabled ? RZI_LORAWAN_ADR_NETWORK : RZI_LORAWAN_ADR_CUSTOM,
				    pinned);
}

static int fake_get_data_rate(enum rzi_lorawan_data_rate *value)
{
	*value = data_rate;
	return 0;
}

static int fake_set_data_rate(enum rzi_lorawan_data_rate value)
{
	uint8_t pinned[RZI_LORAWAN_DR_DISTRIBUTION_LENGTH];

	memset(pinned, value, sizeof(pinned));
	return fake_set_adr_profile(RZI_LORAWAN_ADR_CUSTOM, pinned);
}

static int fake_get_enabled_data_rates(uint16_t *mask)
{
	*mask = US915_UPLINK_DRS;
	return 0;
}

static int fake_get_adr_profile(enum rzi_lorawan_adr_profile *profile)
{
	*profile = adr_profile;
	return 0;
}

static int fake_get_nb_trans(uint8_t *value)
{
	*value = nb_trans;
	return 0;
}

static int fake_set_nb_trans(uint8_t value)
{
	int rc = require_joined();

	if (rc != 0) {
		return rc;
	}
	if (adr_profile == RZI_LORAWAN_ADR_NETWORK) {
		return -RZI_ERR_INVALID;
	}
	nb_trans = value;
	return 0;
}

static int fake_get_public_network(bool *enabled)
{
	*enabled = public_network;
	return 0;
}

static int fake_set_public_network(bool enabled)
{
	public_network = enabled;
	return 0;
}

static int fake_get_lbt(bool *enabled)
{
	*enabled = lbt_enabled;
	return 0;
}

static int fake_set_lbt(bool enabled)
{
	lbt_enabled = enabled;
	return 0;
}

static int fake_get_lbt_rssi(int16_t *value)
{
	*value = lbt_rssi_dbm;
	return 0;
}

static int fake_set_lbt_rssi(int16_t value)
{
	lbt_rssi_dbm = value;
	return 0;
}

static int fake_get_lbt_scan_time(uint32_t *value)
{
	*value = lbt_scan_time_ms;
	return 0;
}

static int fake_set_lbt_scan_time(uint32_t value)
{
	lbt_scan_time_ms = value;
	return 0;
}

static int fake_query_tx_possible(size_t size)
{
	return size <= 51U ? 0 : -RZI_ERR_TOO_LARGE;
}

static const struct rzi_lorawan_network_ops fake_network_ops = {
	.get_adr = fake_get_adr,
	.set_adr = fake_set_adr,
	.get_data_rate = fake_get_data_rate,
	.set_data_rate = fake_set_data_rate,
	.get_enabled_data_rates = fake_get_enabled_data_rates,
	.get_adr_profile = fake_get_adr_profile,
	.set_adr_profile = fake_set_adr_profile,
	.get_nb_trans = fake_get_nb_trans,
	.set_nb_trans = fake_set_nb_trans,
	.get_public_network = fake_get_public_network,
	.set_public_network = fake_set_public_network,
	.get_lbt = fake_get_lbt,
	.set_lbt = fake_set_lbt,
	.get_lbt_rssi = fake_get_lbt_rssi,
	.set_lbt_rssi = fake_set_lbt_rssi,
	.get_lbt_scan_time = fake_get_lbt_scan_time,
	.set_lbt_scan_time = fake_set_lbt_scan_time,
	.send_empty = fake_send_empty,
	.query_tx_possible = fake_query_tx_possible,
};

static int fake_get_ping_slot(uint8_t *value)
{
	*value = ping_slot;
	return 0;
}

static int fake_set_ping_slot(uint8_t value)
{
	ping_slot = value;
	return 0;
}

static int fake_get_beacon_frequency(uint32_t *value)
{
	*value = beacon_frequency_hz;
	return 0;
}

static int fake_get_beacon_time(uint32_t *value)
{
	*value = beacon_time;
	return 0;
}

static int fake_get_beacon_data_rate(enum rzi_lorawan_data_rate *value)
{
	*value = beacon_data_rate;
	return 0;
}

static int fake_get_beacon_gateway(struct rzi_lorawan_beacon_gateway *value)
{
	*value = beacon_gateway;
	return 0;
}

static int fake_get_class_b_state(enum rzi_lorawan_class_b_state *value)
{
	*value = class_b_state;
	return 0;
}

static int fake_stop_class_b(void)
{
	bool changed = class_b_state != RZI_LORAWAN_CLASS_B_IDLE;

	device_class = RZI_LORAWAN_CLASS_A;
	class_b_state = RZI_LORAWAN_CLASS_B_IDLE;
	if (changed) {
		const struct rzi_lorawan_backend_event event = {
			.type = RZI_LORAWAN_BACKEND_CLASS_B,
			.class_b = class_b_state,
		};

		fake_sink(&event);
	}
	return 0;
}

static const struct rzi_lorawan_class_b_ops fake_class_b_ops = {
	.get_ping_slot_periodicity = fake_get_ping_slot,
	.set_ping_slot_periodicity = fake_set_ping_slot,
	.get_beacon_frequency = fake_get_beacon_frequency,
	.get_beacon_time = fake_get_beacon_time,
	.get_beacon_data_rate = fake_get_beacon_data_rate,
	.get_beacon_gateway = fake_get_beacon_gateway,
	.get_state = fake_get_class_b_state,
	.stop = fake_stop_class_b,
};

static int fake_get_net_id(uint32_t *value)
{
	*value = net_id;
	return 0;
}

static int fake_get_dev_nonce(uint16_t *value)
{
	*value = dev_nonce;
	return 0;
}

static int fake_set_dev_nonce(uint16_t value)
{
	dev_nonce = value;
	return 0;
}

static int fake_is_busy(bool *value)
{
	*value = tx_pending;
	return 0;
}

uint32_t fake_otaa_dev_addr = 0x01234567U;
uint8_t fake_otaa_nwk_skey[16] = {
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
};
uint8_t fake_otaa_app_skey[16] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
};

static int fake_get_dev_addr(uint32_t *value)
{
	*value = fake_otaa_dev_addr;
	return 0;
}

static int fake_get_nwk_skey(uint8_t *key)
{
	memcpy(key, fake_otaa_nwk_skey, sizeof(fake_otaa_nwk_skey));
	return 0;
}

static int fake_get_app_skey(uint8_t *key)
{
	memcpy(key, fake_otaa_app_skey, sizeof(fake_otaa_app_skey));
	return 0;
}

static const struct rzi_lorawan_session_ops fake_session_ops = {
	.get_net_id = fake_get_net_id,
	.get_dev_nonce = fake_get_dev_nonce,
	.set_dev_nonce = fake_set_dev_nonce,
	.get_dev_addr = fake_get_dev_addr,
	.get_nwk_skey = fake_get_nwk_skey,
	.get_app_skey = fake_get_app_skey,
};

static int fake_link_check_request(void)
{
	struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_LINK_CHECK,
		.link_check.demod_margin = 10,
		.link_check.gateway_count = 2,
	};

	fake_sink(&event);
	return 0;
}

static int fake_device_time_request(void)
{
	struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_DEVICE_TIME,
		.error = 0,
	};

	network_time.gps_seconds = 1000;
	network_time.gps_subseconds = 1;
	fake_sink(&event);
	return 0;
}

static int fake_get_network_time(struct rzi_lorawan_network_time *time)
{
	*time = network_time;
	return 0;
}

static const struct rzi_lorawan_mac_ops fake_mac_ops = {
	.request_link_check = fake_link_check_request,
	.request_device_time = fake_device_time_request,
	.get_network_time = fake_get_network_time,
};

static int fake_multicast_add(const struct rzi_lorawan_multicast_session *session)
{
	int8_t group = session->group_id;

	if (group < 0) {
		for (group = 0; group < 4; ++group) {
			if (!multicast_used[group]) {
				break;
			}
		}
	}
	if (group < 0 || group > 3 || multicast_used[group]) {
		return -RZI_ERR_NO_RESOURCE;
	}
	multicast[group] = *session;
	multicast[group].group_id = group;
	memset(multicast[group].application_session_key, 0,
	       sizeof(multicast[group].application_session_key));
	memset(multicast[group].network_session_key, 0,
	       sizeof(multicast[group].network_session_key));
	multicast_used[group] = true;
	return 0;
}

static int fake_multicast_remove(uint32_t dev_addr)
{
	for (size_t i = 0; i < ARRAY_SIZE(multicast); ++i) {
		if (multicast_used[i] && multicast[i].dev_addr == dev_addr) {
			multicast_used[i] = false;
			return 0;
		}
	}
	return -RZI_ERR_NOT_FOUND;
}

static int fake_multicast_count(size_t *count)
{
	size_t n = 0;

	for (size_t i = 0; i < ARRAY_SIZE(multicast); ++i) {
		if (multicast_used[i]) {
			n++;
		}
	}
	*count = n;
	return 0;
}

static int fake_multicast_get(size_t index, struct rzi_lorawan_multicast_session *session)
{
	size_t n = 0;

	for (size_t i = 0; i < ARRAY_SIZE(multicast); ++i) {
		if (!multicast_used[i]) {
			continue;
		}
		if (n == index) {
			*session = multicast[i];
			return 0;
		}
		n++;
	}
	return -RZI_ERR_INVALID;
}

static int fake_multicast_clear(void)
{
	memset(multicast_used, 0, sizeof(multicast_used));
	return 0;
}

static const struct rzi_lorawan_multicast_ops fake_multicast_ops = {
	.add = fake_multicast_add,
	.remove = fake_multicast_remove,
	.get_count = fake_multicast_count,
	.get = fake_multicast_get,
	.clear = fake_multicast_clear,
};

static int fake_get_cert_mode(bool *enabled)
{
	*enabled = cert_mode;
	return 0;
}

static int fake_set_cert_mode(bool enabled)
{
	cert_mode = enabled;
	return 0;
}

static int fake_get_cert_port(bool *enabled)
{
	*enabled = cert_port;
	return 0;
}

static int fake_set_cert_port(bool enabled)
{
	cert_port = enabled;
	return 0;
}

static const struct rzi_lorawan_certification_ops fake_cert_ops = {
	.get_mode = fake_get_cert_mode,
	.set_mode = fake_set_cert_mode,
	.get_port_enabled = fake_get_cert_port,
	.set_port_enabled = fake_set_cert_port,
};

const struct rzi_lorawan_backend_api rzi_lorawan_backend = {
	.capabilities = FAKE_CAPABILITIES,
	.base.start = fake_start,
	.base.join = fake_join,
	.base.leave = fake_leave,
	.base.send = fake_send,
	.base.set_class = fake_set_class,
	.base.get_class = fake_get_class,
	.base.is_joined = fake_is_joined,
	.base.is_busy = fake_is_busy,
	.network = &fake_network_ops,
	.class_b = &fake_class_b_ops,
	.session = &fake_session_ops,
	.mac = &fake_mac_ops,
	.multicast = &fake_multicast_ops,
	.certification = &fake_cert_ops,
};
