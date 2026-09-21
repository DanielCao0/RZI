/* SPDX-License-Identifier: Apache-2.0 */

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

rzi_lorawan_event_sink_t fake_sink;

static enum rzi_lorawan_class device_class = RZI_LORAWAN_CLASS_A;
static bool adr_enabled = true;
static enum rzi_lorawan_data_rate data_rate = RZI_LORAWAN_DR_0;
static uint8_t tx_power;
static bool duty_cycle = true;
static uint32_t rx1_delay_ms = 1000;
static uint32_t rx2_delay_ms = 2000;
static enum rzi_lorawan_data_rate rx2_data_rate = RZI_LORAWAN_DR_0;
static uint32_t rx2_frequency_hz = 869525000;
static uint32_t join_delay1_ms = 5000;
static uint32_t join_delay2_ms = 6000;
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
static uint16_t channel_mask[RZI_LORAWAN_CHANNEL_MASK_WORDS] = {0xffff, 0xffff, 0xffff,
								0xffff, 0xffff, 0x00ff};
static uint8_t sub_band;
static uint32_t fixed_channel_hz;
static uint32_t net_id;
static uint16_t dev_nonce = 1;
static bool busy;
static struct rzi_lorawan_network_time network_time;
static bool cert_mode;
static bool cert_port = true;
static struct rzi_lorawan_multicast_session multicast[4];
static bool multicast_used[4];
static bool session_joined;

static int fake_start(enum rzi_lorawan_region region, bool join_backoff_bypass,
		      rzi_lorawan_event_sink_t event_sink)
{
	const struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_READY,
	};

	zassert_equal(region, RZI_LORAWAN_REGION_US_915);
	zassert_true(join_backoff_bypass);
	fake_sink = event_sink;
	session_joined = false;
	fake_sink(&event);
	return 0;
}

static int fake_join(const struct rzi_lorawan_join_config *config)
{
	const struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_JOINED,
	};

	if (config->activation != RZI_LORAWAN_ACTIVATION_OTAA) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	zassert_equal(config->activation, RZI_LORAWAN_ACTIVATION_OTAA);
	session_joined = true;
	fake_sink(&event);
	return 0;
}

static int fake_leave(void)
{
	session_joined = false;
	return 0;
}

static int fake_send(uint8_t port, const uint8_t *data, size_t size,
		     enum rzi_lorawan_message_type type)
{
	const struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_TX_DONE,
		.tx_status = RZI_LORAWAN_TX_ACKED,
	};

	zassert_equal(port, 10);
	zassert_equal(size, 2);
	zassert_equal(data[0], 0x12);
	zassert_equal(type, RZI_LORAWAN_MSG_CONFIRMED);
	fake_sink(&event);
	return 0;
}

static int fake_set_class(enum rzi_lorawan_class requested)
{
	device_class = requested;
	if (requested == RZI_LORAWAN_CLASS_B) {
		class_b_state = RZI_LORAWAN_CLASS_B_ACTIVE;
	} else if (requested == RZI_LORAWAN_CLASS_A) {
		class_b_state = RZI_LORAWAN_CLASS_B_IDLE;
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

static int fake_get_adr(bool *enabled)
{
	*enabled = adr_enabled;
	return 0;
}

static int fake_set_adr(bool enabled)
{
	adr_enabled = enabled;
	return 0;
}

static int fake_get_data_rate(enum rzi_lorawan_data_rate *value)
{
	*value = data_rate;
	return 0;
}

static int fake_set_data_rate(enum rzi_lorawan_data_rate value)
{
	data_rate = value;
	return 0;
}

static int fake_get_tx_power(uint8_t *value)
{
	*value = tx_power;
	return 0;
}

static int fake_set_tx_power(uint8_t value)
{
	tx_power = value;
	return 0;
}

static int fake_get_duty_cycle(bool *enabled)
{
	*enabled = duty_cycle;
	return 0;
}

static int fake_set_duty_cycle(bool enabled)
{
	duty_cycle = enabled;
	return 0;
}

static int fake_get_rx1_delay(uint32_t *value)
{
	*value = rx1_delay_ms;
	return 0;
}

static int fake_set_rx1_delay(uint32_t value)
{
	rx1_delay_ms = value;
	return 0;
}

static int fake_get_rx2_delay(uint32_t *value)
{
	*value = rx2_delay_ms;
	return 0;
}

static int fake_set_rx2_delay(uint32_t value)
{
	rx2_delay_ms = value;
	return 0;
}

static int fake_get_rx2_data_rate(enum rzi_lorawan_data_rate *value)
{
	*value = rx2_data_rate;
	return 0;
}

static int fake_set_rx2_data_rate(enum rzi_lorawan_data_rate value)
{
	rx2_data_rate = value;
	return 0;
}

static int fake_get_rx2_frequency(uint32_t *value)
{
	*value = rx2_frequency_hz;
	return 0;
}

static int fake_set_rx2_frequency(uint32_t value)
{
	rx2_frequency_hz = value;
	return 0;
}

static int fake_get_join_delay1(uint32_t *value)
{
	*value = join_delay1_ms;
	return 0;
}

static int fake_set_join_delay1(uint32_t value)
{
	join_delay1_ms = value;
	return 0;
}

static int fake_get_join_delay2(uint32_t *value)
{
	*value = join_delay2_ms;
	return 0;
}

static int fake_set_join_delay2(uint32_t value)
{
	join_delay2_ms = value;
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
	device_class = RZI_LORAWAN_CLASS_A;
	class_b_state = RZI_LORAWAN_CLASS_B_IDLE;
	return 0;
}

static const struct rzi_lorawan_network_ops fake_network_ops = {
	.get_adr = fake_get_adr,
	.set_adr = fake_set_adr,
	.get_data_rate = fake_get_data_rate,
	.set_data_rate = fake_set_data_rate,
	.get_tx_power = fake_get_tx_power,
	.set_tx_power = fake_set_tx_power,
	.get_duty_cycle = fake_get_duty_cycle,
	.set_duty_cycle = fake_set_duty_cycle,
	.get_rx1_delay = fake_get_rx1_delay,
	.set_rx1_delay = fake_set_rx1_delay,
	.get_rx2_delay = fake_get_rx2_delay,
	.set_rx2_delay = fake_set_rx2_delay,
	.get_rx2_data_rate = fake_get_rx2_data_rate,
	.set_rx2_data_rate = fake_set_rx2_data_rate,
	.get_rx2_frequency = fake_get_rx2_frequency,
	.set_rx2_frequency = fake_set_rx2_frequency,
	.get_join_accept_delay1 = fake_get_join_delay1,
	.set_join_accept_delay1 = fake_set_join_delay1,
	.get_join_accept_delay2 = fake_get_join_delay2,
	.set_join_accept_delay2 = fake_set_join_delay2,
	.get_public_network = fake_get_public_network,
	.set_public_network = fake_set_public_network,
	.get_lbt = fake_get_lbt,
	.set_lbt = fake_set_lbt,
	.get_lbt_rssi = fake_get_lbt_rssi,
	.set_lbt_rssi = fake_set_lbt_rssi,
	.get_lbt_scan_time = fake_get_lbt_scan_time,
	.set_lbt_scan_time = fake_set_lbt_scan_time,
};

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

static int fake_get_channel_mask(uint16_t *mask, size_t words)
{
	size_t copy = MIN(words, ARRAY_SIZE(channel_mask));

	memcpy(mask, channel_mask, copy * sizeof(uint16_t));
	return 0;
}

static int fake_set_channel_mask(const uint16_t *mask, size_t words)
{
	size_t copy = MIN(words, ARRAY_SIZE(channel_mask));

	memset(channel_mask, 0, sizeof(channel_mask));
	memcpy(channel_mask, mask, copy * sizeof(uint16_t));
	return 0;
}

static int fake_get_sub_band(uint8_t *value)
{
	*value = sub_band;
	return 0;
}

static int fake_set_sub_band(uint8_t value)
{
	sub_band = value;
	return 0;
}

static int fake_get_fixed_channel(uint32_t *value)
{
	*value = fixed_channel_hz;
	return 0;
}

static int fake_set_fixed_channel(uint32_t value)
{
	fixed_channel_hz = value;
	return 0;
}

static const struct rzi_lorawan_channel_ops fake_channel_ops = {
	.get_channel_mask = fake_get_channel_mask,
	.set_channel_mask = fake_set_channel_mask,
	.get_sub_band = fake_get_sub_band,
	.set_sub_band = fake_set_sub_band,
	.get_fixed_channel = fake_get_fixed_channel,
	.set_fixed_channel = fake_set_fixed_channel,
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

static int fake_query_tx_possible(size_t size)
{
	return size <= 51U ? 0 : -RZI_ERR_TOO_LARGE;
}

static int fake_info_is_busy(bool *value)
{
	*value = busy;
	return 0;
}

static const struct rzi_lorawan_session_ops fake_session_ops = {
	.get_net_id = fake_get_net_id,
	.get_dev_nonce = fake_get_dev_nonce,
	.set_dev_nonce = fake_set_dev_nonce,
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
	.start = fake_start,
	.join = fake_join,
	.leave = fake_leave,
	.send = fake_send,
	.set_class = fake_set_class,
	.get_class = fake_get_class,
	.is_joined = fake_is_joined,
	.query_tx_possible = fake_query_tx_possible,
	.is_busy = fake_info_is_busy,
	.network = &fake_network_ops,
	.channel = &fake_channel_ops,
	.class_b = &fake_class_b_ops,
	.session = &fake_session_ops,
	.mac = &fake_mac_ops,
	.multicast = &fake_multicast_ops,
	.certification = &fake_cert_ops,
};
