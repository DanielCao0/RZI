/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/ztest.h>

#include <rzi/capabilities.h>
#include <rzi/lorawan/certification.h>
#include <rzi/lorawan/lorawan.h>
#include <rzi/lorawan/multicast.h>
#include <rzi/version.h>

#include "backend/lorawan_backend.h"

K_SEM_DEFINE(callback_sem, 0, 8);

extern rzi_lorawan_event_sink_t fake_sink;
extern int fake_set_class_error;

struct callback_stats {
	atomic_t ready;
	atomic_t joined;
	atomic_t join_failed;
	atomic_t uplink;
	atomic_t downlink;
	atomic_t errors;
	atomic_t payload_valid;
	atomic_t link_checks;
	atomic_t times;
	atomic_t states;
	atomic_t class_b;
	atomic_t last_join_error;
	atomic_t last_tx_error;
	atomic_t last_device_time_error;
	atomic_t last_state;
	atomic_t last_class_b_state;
};

static void on_event(const struct rzi_lorawan_event *event, void *user_data)
{
	struct callback_stats *stats = user_data;

	switch (event->type) {
	case RZI_LORAWAN_EVENT_READY:
		atomic_inc(&stats->ready);
		k_sem_give(&callback_sem);
		break;
	case RZI_LORAWAN_EVENT_JOINED:
		atomic_inc(&stats->joined);
		k_sem_give(&callback_sem);
		break;
	case RZI_LORAWAN_EVENT_JOIN_FAILED:
		atomic_set(&stats->last_join_error, event->error);
		atomic_inc(&stats->join_failed);
		k_sem_give(&callback_sem);
		break;
	case RZI_LORAWAN_EVENT_TX_DONE:
		zassert_equal(event->tx.status, RZI_LORAWAN_TX_ACKED);
		atomic_set(&stats->last_tx_error, event->tx.error);
		atomic_inc(&stats->uplink);
		k_sem_give(&callback_sem);
		break;
	case RZI_LORAWAN_EVENT_DOWNLINK:
		zassert_equal(event->downlink.rssi_dbm, -42);
		zassert_equal(event->downlink.snr_quarter_db, 12);
		zassert_equal(event->downlink.flags, 0x55);
		if (event->downlink.port == 3 && event->downlink.size == 2 &&
		    event->downlink.data[0] == 0xab && event->downlink.data[1] == 0xcd) {
			atomic_set(&stats->payload_valid, 1);
		}
		atomic_inc(&stats->downlink);
		k_sem_give(&callback_sem);
		break;
	case RZI_LORAWAN_EVENT_ERROR:
		zassert_equal(event->error, -RZI_ERR_IO);
		atomic_inc(&stats->errors);
		k_sem_give(&callback_sem);
		break;
	case RZI_LORAWAN_EVENT_LINK_CHECK:
		zassert_equal(event->link_check.demod_margin, 10);
		zassert_equal(event->link_check.gateway_count, 2);
		atomic_inc(&stats->link_checks);
		k_sem_give(&callback_sem);
		break;
	case RZI_LORAWAN_EVENT_DEVICE_TIME:
		atomic_set(&stats->last_device_time_error, event->error);
		atomic_inc(&stats->times);
		k_sem_give(&callback_sem);
		break;
	case RZI_LORAWAN_EVENT_STATE_CHANGED:
		atomic_set(&stats->last_state, event->state);
		atomic_inc(&stats->states);
		break;
	case RZI_LORAWAN_EVENT_CLASS_B:
		atomic_set(&stats->last_class_b_state, event->class_b);
		atomic_inc(&stats->class_b);
		k_sem_give(&callback_sem);
		break;
	default:
		break;
	}
}

static const struct rzi_lorawan_callbacks callbacks = {
	.on_event = on_event,
};

static void wait_for_callbacks(unsigned int count)
{
	for (unsigned int i = 0; i < count; ++i) {
		zassert_ok(k_sem_take(&callback_sem, K_SECONDS(1)));
	}
}

ZTEST(rzi_lorawan_service, test_sdk_metadata_and_feature_discovery)
{
	zassert_equal(strcmp(rzi_version_get_string(), RZI_VERSION_STRING), 0);
	zassert_equal(rzi_get_capabilities(), RZI_CAP_LORAWAN);
	zassert_not_null(rzi_lorawan_backend.multicast);
	zassert_true((rzi_lorawan_get_capabilities() & RZI_LORAWAN_CAP_NETWORK) != 0U);
}

ZTEST(rzi_lorawan_service, test_lifecycle_and_multiple_subscribers)
{
	struct callback_stats first = {0};
	struct callback_stats second = {0};
	struct rzi_lorawan_callbacks first_callbacks = callbacks;
	struct rzi_lorawan_callbacks second_callbacks = callbacks;
	rzi_lorawan_callback_handle_t first_handle;
	rzi_lorawan_callback_handle_t second_handle;
	const struct rzi_lorawan_join_config otaa = {
		.activation = RZI_LORAWAN_ACTIVATION_OTAA,
	};
	const struct rzi_lorawan_join_config abp = {
		.activation = RZI_LORAWAN_ACTIVATION_ABP,
	};
	const uint8_t payload[] = {0x12, 0x34};
	bool joined = false;

	first_callbacks.user_data = &first;
	second_callbacks.user_data = &second;
	zassert_ok(rzi_lorawan_register_callbacks(&first_callbacks, &first_handle));
	zassert_ok(rzi_lorawan_register_callbacks(&second_callbacks, &second_handle));
	zassert_not_equal(first_handle, second_handle);
	zassert_equal(rzi_lorawan_register_callbacks(&callbacks, &second_handle),
		      -RZI_ERR_NO_RESOURCE);

	zassert_ok(rzi_lorawan_set_region(RZI_LORAWAN_REGION_US_915));
	zassert_ok(rzi_lorawan_set_join_backoff_bypass(true));
	zassert_ok(rzi_lorawan_start());
	wait_for_callbacks(2);
	zassert_equal(atomic_get(&first.ready), 1);
	zassert_equal(atomic_get(&second.ready), 1);
	zassert_equal(atomic_get(&first.states), 1);
	zassert_equal(atomic_get(&first.last_state), RZI_LORAWAN_STATE_STARTING);
	zassert_equal(rzi_lorawan_start(), -RZI_ERR_ALREADY);
	zassert_equal(rzi_lorawan_send(10, payload, sizeof(payload), RZI_LORAWAN_MSG_CONFIRMED),
		      -RZI_ERR_NOT_JOINED);

	zassert_equal(rzi_lorawan_join(&abp), -RZI_ERR_NOT_SUPPORTED);
	zassert_ok(rzi_lorawan_join(&otaa));
	wait_for_callbacks(2);
	zassert_equal(atomic_get(&first.joined), 1);
	zassert_equal(atomic_get(&second.joined), 1);
	zassert_equal(atomic_get(&second.states), 2);
	zassert_equal(atomic_get(&second.last_state), RZI_LORAWAN_STATE_JOINING);

	zassert_ok(rzi_lorawan_unregister_callbacks(first_handle));
	zassert_ok(rzi_lorawan_send(10, payload, sizeof(payload), RZI_LORAWAN_MSG_CONFIRMED));
	wait_for_callbacks(1);
	zassert_equal(atomic_get(&first.uplink), 0);
	zassert_equal(atomic_get(&second.uplink), 1);
	zassert_equal(atomic_get(&second.last_tx_error), 0);

	{
		const struct rzi_lorawan_backend_event downlink = {
			.type = RZI_LORAWAN_BACKEND_DOWNLINK,
			.downlink.port = 3,
			.downlink.size = 2,
			.downlink.rssi_dbm = -42,
			.downlink.snr_quarter_db = 12,
			.downlink.flags = 0x55,
			.downlink.data = {0xab, 0xcd},
		};
		const struct rzi_lorawan_backend_event tx_error = {
			.type = RZI_LORAWAN_BACKEND_TX_DONE,
			.error = -RZI_ERR_IO,
			.tx_status = RZI_LORAWAN_TX_ACKED,
		};
		const struct rzi_lorawan_backend_event error = {
			.type = RZI_LORAWAN_BACKEND_ERROR,
			.error = -RZI_ERR_IO,
		};
		const struct rzi_lorawan_backend_event join_failed = {
			.type = RZI_LORAWAN_BACKEND_JOIN_FAILED,
		};

		fake_sink(&downlink);
		wait_for_callbacks(1);
		fake_sink(&tx_error);
		wait_for_callbacks(1);
		fake_sink(&error);
		wait_for_callbacks(1);
		fake_sink(&join_failed);
		wait_for_callbacks(1);
	}
	zassert_equal(atomic_get(&second.downlink), 1);
	zassert_true(atomic_get(&second.payload_valid));
	zassert_equal(atomic_get(&second.uplink), 2);
	zassert_equal(atomic_get(&second.last_tx_error), -RZI_ERR_IO);
	zassert_equal(atomic_get(&second.errors), 1);
	zassert_equal(atomic_get(&second.join_failed), 1);
	zassert_equal(atomic_get(&second.last_join_error), -RZI_ERR_TIMEOUT);

	zassert_ok(rzi_lorawan_set_class(RZI_LORAWAN_CLASS_A));
	zassert_ok(rzi_lorawan_set_class(RZI_LORAWAN_CLASS_C));
	zassert_ok(rzi_lorawan_set_class(RZI_LORAWAN_CLASS_A));
	zassert_ok(rzi_lorawan_set_class(RZI_LORAWAN_CLASS_B));
	wait_for_callbacks(1);
	zassert_equal(atomic_get(&second.class_b), 1);
	zassert_equal(atomic_get(&second.last_class_b_state), RZI_LORAWAN_CLASS_B_ACQUIRING_BEACON);
	zassert_ok(rzi_lorawan_set_class(RZI_LORAWAN_CLASS_B));
	k_sleep(K_MSEC(20));
	zassert_equal(atomic_get(&second.class_b), 1);
	zassert_equal(k_sem_take(&callback_sem, K_NO_WAIT), -EBUSY);
	fake_set_class_error = -RZI_ERR_IO;
	zassert_equal(rzi_lorawan_set_class(RZI_LORAWAN_CLASS_A), -RZI_ERR_IO);
	fake_set_class_error = 0;
	{
		enum rzi_lorawan_class device_class;
		const struct rzi_lorawan_backend_event active = {
			.type = RZI_LORAWAN_BACKEND_CLASS_B,
			.class_b = RZI_LORAWAN_CLASS_B_ACTIVE,
		};

		zassert_ok(rzi_lorawan_get_class(&device_class));
		zassert_equal(device_class, RZI_LORAWAN_CLASS_B);
		fake_sink(&active);
		wait_for_callbacks(1);
		zassert_equal(atomic_get(&second.last_class_b_state), RZI_LORAWAN_CLASS_B_ACTIVE);
	}
	zassert_ok(rzi_lorawan_stop_class_b());
	wait_for_callbacks(1);
	zassert_equal(atomic_get(&second.last_class_b_state), RZI_LORAWAN_CLASS_B_IDLE);
	zassert_ok(rzi_lorawan_is_joined(&joined));
	zassert_true(joined);

	{
		enum rzi_lorawan_region region;
		enum rzi_lorawan_class device_class;
		enum rzi_lorawan_data_rate data_rate;
		bool flag = true;
		uint8_t tx_power = 0xff;
		uint32_t delay = 0;
		uint16_t mask[RZI_LORAWAN_CHANNEL_MASK_WORDS] = {0};
		uint8_t sub_band = 9;
		int16_t rssi = 0;
		const char *version = NULL;
		size_t count = 0;
		struct rzi_lorawan_multicast_session session = {
			.device_class = RZI_LORAWAN_CLASS_C,
			.dev_addr = 0x11223344,
			.frequency_hz = 869525000,
			.data_rate = RZI_LORAWAN_DR_8,
			.group_id = -1,
		};
		struct rzi_lorawan_multicast_session listed;
		struct rzi_lorawan_network_time time;
		enum rzi_lorawan_link_check_mode link_mode;

		zassert_ok(rzi_lorawan_get_region(&region));
		zassert_equal(region, RZI_LORAWAN_REGION_US_915);
		zassert_ok(rzi_lorawan_set_adr(false));
		zassert_ok(rzi_lorawan_get_adr(&flag));
		zassert_false(flag);
		zassert_equal(rzi_lorawan_set_data_rate(RZI_LORAWAN_DR_8), -RZI_ERR_INVALID);
		zassert_equal(rzi_lorawan_set_data_rate(RZI_LORAWAN_DR_14), -RZI_ERR_INVALID);
		zassert_ok(rzi_lorawan_set_data_rate(RZI_LORAWAN_DR_3));
		zassert_ok(rzi_lorawan_get_data_rate(&data_rate));
		zassert_equal(data_rate, RZI_LORAWAN_DR_3);
		zassert_equal(rzi_lorawan_set_rx2_data_rate(RZI_LORAWAN_DR_0), -RZI_ERR_INVALID);
		zassert_ok(rzi_lorawan_set_rx2_data_rate(RZI_LORAWAN_DR_8));
		zassert_ok(rzi_lorawan_set_tx_power(4));
		zassert_ok(rzi_lorawan_get_tx_power(&tx_power));
		zassert_equal(tx_power, 4);
		zassert_ok(rzi_lorawan_set_duty_cycle(false));
		zassert_ok(rzi_lorawan_get_duty_cycle(&flag));
		zassert_false(flag);
		zassert_ok(rzi_lorawan_set_rx1_delay(1500));
		zassert_ok(rzi_lorawan_get_rx1_delay(&delay));
		zassert_equal(delay, 1500);
		zassert_ok(rzi_lorawan_set_public_network(false));
		zassert_ok(rzi_lorawan_get_public_network(&flag));
		zassert_false(flag);
		zassert_ok(rzi_lorawan_set_lbt(true));
		zassert_ok(rzi_lorawan_get_lbt(&flag));
		zassert_true(flag);
		mask[0] = 0x00ff;
		zassert_ok(rzi_lorawan_set_channel_mask(mask, ARRAY_SIZE(mask)));
		memset(mask, 0, sizeof(mask));
		zassert_ok(rzi_lorawan_get_channel_mask(mask, ARRAY_SIZE(mask)));
		zassert_equal(mask[0], 0x00ff);
		zassert_ok(rzi_lorawan_set_sub_band(2));
		zassert_ok(rzi_lorawan_get_sub_band(&sub_band));
		zassert_equal(sub_band, 2);
		zassert_ok(rzi_lorawan_get_last_rssi(&rssi));
		zassert_ok(rzi_lorawan_get_protocol_version(&version));
		zassert_ok(strcmp(version, "LoRaWAN 1.0.4"));
		zassert_ok(rzi_lorawan_query_tx_possible(10));
		zassert_equal(rzi_lorawan_query_tx_possible(200), -RZI_ERR_TOO_LARGE);
		zassert_ok(rzi_lorawan_set_ping_slot_periodicity(3));
		zassert_ok(rzi_lorawan_get_class(&device_class));
		zassert_equal(device_class, RZI_LORAWAN_CLASS_A);
		zassert_ok(rzi_lorawan_add_multicast_session(&session));
		zassert_ok(rzi_lorawan_get_multicast_count(&count));
		zassert_equal(count, 1);
		zassert_ok(rzi_lorawan_get_multicast_session(0, &listed));
		zassert_equal(listed.dev_addr, 0x11223344);
		zassert_ok(rzi_lorawan_remove_multicast_session(0x11223344));
		zassert_ok(rzi_lorawan_set_certification_mode(true));
		zassert_ok(rzi_lorawan_get_certification_mode(&flag));
		zassert_true(flag);
		zassert_ok(rzi_lorawan_request_link_check(RZI_LORAWAN_LINK_CHECK_ONCE));
		wait_for_callbacks(1);
		zassert_ok(rzi_lorawan_get_link_check_mode(&link_mode));
		zassert_equal(link_mode, RZI_LORAWAN_LINK_CHECK_DISABLED);
		zassert_ok(rzi_lorawan_request_device_time(true));
		wait_for_callbacks(1);
		zassert_equal(atomic_get(&second.last_device_time_error), 0);
		zassert_ok(rzi_lorawan_get_network_time(&time));
		zassert_equal(time.gps_seconds, 1000);
	}
	{
		const struct rzi_lorawan_backend_event time_failed = {
			.type = RZI_LORAWAN_BACKEND_DEVICE_TIME,
			.error = -RZI_ERR_TIMEOUT,
		};

		fake_sink(&time_failed);
		wait_for_callbacks(1);
		zassert_equal(atomic_get(&second.last_device_time_error), -RZI_ERR_TIMEOUT);
	}

	zassert_ok(rzi_lorawan_unregister_callbacks(second_handle));
	zassert_equal(rzi_lorawan_unregister_callbacks(second_handle), -RZI_ERR_NOT_FOUND);
}

ZTEST(rzi_lorawan_service, test_oversized_uplink_is_too_large)
{
	uint8_t too_big[RZI_LORAWAN_MAX_PAYLOAD + 1U] = {0};

	zassert_equal(rzi_lorawan_send(10, too_big, sizeof(too_big), RZI_LORAWAN_MSG_UNCONFIRMED),
		      -RZI_ERR_TOO_LARGE);
}

ZTEST_SUITE(rzi_lorawan_service, NULL, NULL, NULL, NULL, NULL);
