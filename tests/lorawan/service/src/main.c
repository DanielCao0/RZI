/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/ztest.h>

#include <rzi/lorawan/certification.h>
#include <rzi/lorawan/lorawan.h>
#include <rzi/lorawan/multicast.h>
#include <rzi/version.h>

#include "backend/lorawan_backend.h"

K_SEM_DEFINE(callback_sem, 0, 8);

extern rzi_lorawan_event_sink_t fake_sink;
extern int fake_set_class_error;
extern int fake_join_failures;
extern int fake_join_attempts;
extern int fake_leave_calls;
extern bool fake_tx_hold;
extern struct rzi_lorawan_join_config fake_last_join;
void fake_complete_tx(void);
extern uint32_t fake_otaa_dev_addr;
extern uint8_t fake_otaa_nwk_skey[16];
extern uint8_t fake_otaa_app_skey[16];

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
	atomic_t duty_cycle;
	atomic_t no_downlink;
	atomic_t last_error;
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
		atomic_set(&stats->last_tx_error, event->tx.error);
		atomic_inc(&stats->uplink);
		k_sem_give(&callback_sem);
		break;
	case RZI_LORAWAN_EVENT_DOWNLINK:
		zassert_equal(event->downlink.rssi_dbm, -42);
		zassert_equal(event->downlink.snr_quarter_db, 12);
		zassert_equal(event->downlink.flags, RZI_LORAWAN_DOWNLINK_FPENDING);
		zassert_equal(event->downlink.window, RZI_LORAWAN_RX_WINDOW_RX1);
		zassert_equal(event->downlink.data_rate, RZI_LORAWAN_DR_10);
		zassert_equal(event->downlink.frequency_hz, 923300000U);
		if (event->downlink.port == 3 && event->downlink.size == 2 &&
		    event->downlink.data[0] == 0xab && event->downlink.data[1] == 0xcd) {
			atomic_set(&stats->payload_valid, 1);
		}
		atomic_inc(&stats->downlink);
		k_sem_give(&callback_sem);
		break;
	case RZI_LORAWAN_EVENT_ERROR:
		atomic_set(&stats->last_error, event->error);
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
	case RZI_LORAWAN_EVENT_DUTY_CYCLE:
		atomic_set(&stats->duty_cycle, event->duty_cycle_limited ? 1 : 0);
		k_sem_give(&callback_sem);
		break;
	case RZI_LORAWAN_EVENT_NO_DOWNLINK:
		atomic_set(&stats->no_downlink, event->no_downlink);
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
	zassert_not_null(rzi_lorawan_backend.multicast);
	zassert_true((rzi_lorawan_get_capabilities() & RZI_LORAWAN_CAP_NETWORK) != 0U);
}

ZTEST(rzi_lorawan_service, test_lifecycle_and_multiple_subscribers)
{
	struct callback_stats first = {0};
	struct callback_stats second = {0};
	struct rzi_lorawan_callbacks first_callbacks = callbacks;
	struct rzi_lorawan_callbacks second_callbacks = callbacks;
	const uint8_t payload[] = {0x12, 0x34};
	bool joined = false;

	first_callbacks.user_data = &first;
	second_callbacks.user_data = &second;
	zassert_ok(rzi_lorawan_register_callbacks(&first_callbacks));
	zassert_ok(rzi_lorawan_register_callbacks(&second_callbacks));
	zassert_equal(rzi_lorawan_register_callbacks(&first_callbacks), -RZI_ERR_ALREADY);
	zassert_equal(rzi_lorawan_register_callbacks(&callbacks), -RZI_ERR_NO_RESOURCE);

	zassert_ok(rzi_lorawan_set_join_backoff_bypass(true));
	zassert_ok(rzi_lorawan_init(RZI_LORAWAN_REGION_US_915));
	wait_for_callbacks(2);
	zassert_equal(atomic_get(&first.ready), 1);
	zassert_equal(atomic_get(&second.ready), 1);
	zassert_equal(atomic_get(&first.states), 1);
	zassert_equal(atomic_get(&first.last_state), RZI_LORAWAN_STATE_STARTING);
	zassert_equal(rzi_lorawan_init(RZI_LORAWAN_REGION_US_915), -RZI_ERR_ALREADY);
	zassert_equal(rzi_lorawan_send(10, payload, sizeof(payload), RZI_LORAWAN_MSG_CONFIRMED),
		      -RZI_ERR_NOT_JOINED);

	zassert_equal(rzi_lorawan_set_activation(RZI_LORAWAN_ACTIVATION_ABP),
		      -RZI_ERR_NOT_SUPPORTED);
	{
		uint32_t dev_addr = 0xffffffffU;
		uint8_t stored[16];
		uint8_t key[16];
		uint8_t zeros[16] = {0};

		memset(stored, 0x5a, sizeof(stored));
		zassert_ok(rzi_lorawan_set_dev_addr(0xaabbccddU));
		zassert_ok(rzi_lorawan_set_nwk_skey(stored, sizeof(stored)));
		zassert_ok(rzi_lorawan_set_app_skey(stored, sizeof(stored)));
		zassert_ok(rzi_lorawan_get_dev_addr(&dev_addr));
		zassert_equal(dev_addr, 0U);
		memset(key, 0xff, sizeof(key));
		zassert_ok(rzi_lorawan_get_nwk_skey(key, sizeof(key)));
		zassert_mem_equal(key, zeros, sizeof(zeros));
		memset(key, 0xff, sizeof(key));
		zassert_ok(rzi_lorawan_get_app_skey(key, sizeof(key)));
		zassert_mem_equal(key, zeros, sizeof(zeros));
	}
	zassert_ok(rzi_lorawan_join(1, 0, RZI_LORAWAN_JOIN_INTERVAL_DEFAULT, 0));
	wait_for_callbacks(2);
	zassert_equal(atomic_get(&first.joined), 1);
	zassert_equal(atomic_get(&second.joined), 1);
	{
		uint32_t dev_addr = 0;
		uint8_t key[16];

		zassert_ok(rzi_lorawan_get_dev_addr(&dev_addr));
		zassert_equal(dev_addr, fake_otaa_dev_addr);
		zassert_ok(rzi_lorawan_get_nwk_skey(key, sizeof(key)));
		zassert_mem_equal(key, fake_otaa_nwk_skey, sizeof(key));
		zassert_ok(rzi_lorawan_get_app_skey(key, sizeof(key)));
		zassert_mem_equal(key, fake_otaa_app_skey, sizeof(key));
	}
	zassert_equal(atomic_get(&second.states), 2);
	zassert_equal(atomic_get(&second.last_state), RZI_LORAWAN_STATE_JOINING);

	zassert_ok(rzi_lorawan_unregister_callbacks(&first_callbacks));
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
			.downlink.flags = RZI_LORAWAN_DOWNLINK_FPENDING,
			.downlink.window = RZI_LORAWAN_RX_WINDOW_RX1,
			.downlink.data_rate = RZI_LORAWAN_DR_10,
			.downlink.frequency_hz = 923300000U,
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
	zassert_equal(atomic_get(&second.last_error), -RZI_ERR_IO);
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
		uint32_t frequency_hz = 0;
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
		/* RUI3-only parameters LoRa Basics Modem does not expose. */
		zassert_equal(rzi_lorawan_set_rx2_data_rate(RZI_LORAWAN_DR_8),
			      -RZI_ERR_NOT_SUPPORTED);
		zassert_equal(rzi_lorawan_get_rx2_data_rate(&data_rate), -RZI_ERR_NOT_SUPPORTED);
		zassert_equal(rzi_lorawan_set_tx_power(4), -RZI_ERR_NOT_SUPPORTED);
		zassert_equal(rzi_lorawan_get_tx_power(&tx_power), -RZI_ERR_NOT_SUPPORTED);
		zassert_equal(rzi_lorawan_set_duty_cycle(false), -RZI_ERR_NOT_SUPPORTED);
		zassert_equal(rzi_lorawan_get_duty_cycle(&flag), -RZI_ERR_NOT_SUPPORTED);
		zassert_equal(rzi_lorawan_set_rx1_delay(1500), -RZI_ERR_NOT_SUPPORTED);
		zassert_equal(rzi_lorawan_get_rx2_delay(&delay), -RZI_ERR_NOT_SUPPORTED);
		zassert_equal(rzi_lorawan_set_join_accept_delay1(5000), -RZI_ERR_NOT_SUPPORTED);
		zassert_equal(rzi_lorawan_get_rx2_frequency(&frequency_hz), -RZI_ERR_NOT_SUPPORTED);
		zassert_ok(rzi_lorawan_set_public_network(false));
		zassert_ok(rzi_lorawan_get_public_network(&flag));
		zassert_false(flag);
		zassert_ok(rzi_lorawan_set_lbt(true));
		zassert_ok(rzi_lorawan_get_lbt(&flag));
		zassert_true(flag);
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

	zassert_ok(rzi_lorawan_unregister_callbacks(&second_callbacks));
	zassert_equal(rzi_lorawan_unregister_callbacks(&second_callbacks), -RZI_ERR_NOT_FOUND);
}

ZTEST(rzi_lorawan_service, test_oversized_uplink_is_too_large)
{
	uint8_t too_big[RZI_LORAWAN_MAX_PAYLOAD + 1U] = {0};

	zassert_equal(rzi_lorawan_send(10, too_big, sizeof(too_big), RZI_LORAWAN_MSG_UNCONFIRMED),
		      -RZI_ERR_TOO_LARGE);
}

static bool usp_registered;

static void wait_for_count(atomic_t *counter, atomic_val_t count)
{
	for (int i = 0; i < 200 && atomic_get(counter) < count; ++i) {
		k_sleep(K_MSEC(5));
	}
	zassert_equal(atomic_get(counter), count);
}

/* Static so a failed test cannot leave a subscriber pointing at its stack. */
static struct callback_stats usp_stats;
static struct rzi_lorawan_callbacks usp_callbacks;

static struct callback_stats *subscribe(void)
{
	memset(&usp_stats, 0, sizeof(usp_stats));
	usp_callbacks = callbacks;
	usp_callbacks.user_data = &usp_stats;
	zassert_ok(rzi_lorawan_register_callbacks(&usp_callbacks));
	usp_registered = true;
	return &usp_stats;
}

static void unsubscribe_after(void *fixture)
{
	ARG_UNUSED(fixture);
	if (usp_registered) {
		zassert_ok(rzi_lorawan_unregister_callbacks(&usp_callbacks));
		usp_registered = false;
	}
	fake_tx_hold = false;
}

static void ensure_ready_and_left(void)
{
	int rc = rzi_lorawan_set_join_backoff_bypass(true);

	if (rc == 0) {
		zassert_ok(rzi_lorawan_init(RZI_LORAWAN_REGION_US_915));
		zassert_ok(k_sem_take(&callback_sem, K_SECONDS(1)));
	} else {
		zassert_equal(rc, -RZI_ERR_INVALID_STATE);
	}
	zassert_ok(rzi_lorawan_join(0, RZI_LORAWAN_JOIN_KEEP, RZI_LORAWAN_JOIN_KEEP,
				    RZI_LORAWAN_JOIN_KEEP));
	zassert_ok(rzi_lorawan_leave());
	k_sleep(K_MSEC(20));
	k_sem_reset(&callback_sem);
	fake_join_failures = 0;
	fake_join_attempts = 0;
	fake_leave_calls = 0;
}

ZTEST(rzi_lorawan_service, test_stop_join_aborts_pending_attempt)
{
	bool joined = true;

	ensure_ready_and_left();
	fake_join_failures = -1;
	zassert_ok(rzi_lorawan_join(1, 0, RZI_LORAWAN_JOIN_INTERVAL_DEFAULT, 3));
	zassert_equal(rzi_lorawan_join(1, RZI_LORAWAN_JOIN_KEEP, RZI_LORAWAN_JOIN_KEEP,
				       RZI_LORAWAN_JOIN_KEEP),
		      -RZI_ERR_BUSY);
	zassert_ok(rzi_lorawan_join(0, RZI_LORAWAN_JOIN_KEEP, RZI_LORAWAN_JOIN_KEEP,
				    RZI_LORAWAN_JOIN_KEEP));
	zassert_equal(fake_leave_calls, 1);
	zassert_ok(rzi_lorawan_is_joined(&joined));
	zassert_false(joined);

	/* Stopping an idle service does not touch the stack. */
	zassert_ok(rzi_lorawan_join(0, RZI_LORAWAN_JOIN_KEEP, RZI_LORAWAN_JOIN_KEEP,
				    RZI_LORAWAN_JOIN_KEEP));
	zassert_equal(fake_leave_calls, 1);

	/* No more attempts after the stop. */
	fake_join_attempts = 0;
	k_sleep(K_MSEC(30));
	zassert_equal(fake_join_attempts, 0);
}

ZTEST(rzi_lorawan_service, test_usp_join_retries_run_in_the_stack)
{
	struct callback_stats *stats;
	bool joined = false;

	ensure_ready_and_left();
	stats = subscribe();

	/* Two failed attempts are absorbed; the third attempt joins. */
	fake_join_failures = 2;
	zassert_ok(rzi_lorawan_join(1, 0, RZI_LORAWAN_JOIN_INTERVAL_DEFAULT, 3));
	wait_for_count(&stats->joined, 1);
	zassert_equal(atomic_get(&stats->join_failed), 0);
	zassert_equal(fake_join_attempts, 3);
	zassert_equal(fake_leave_calls, 0);
	zassert_ok(rzi_lorawan_is_joined(&joined));
	zassert_true(joined);

	/* With attempts = 2, the third failure ends the sequence and stops the stack. */
	zassert_ok(rzi_lorawan_leave());
	fake_join_attempts = 0;
	fake_leave_calls = 0;
	fake_join_failures = -1;
	zassert_ok(rzi_lorawan_join(1, 0, RZI_LORAWAN_JOIN_INTERVAL_DEFAULT, 2));
	wait_for_count(&stats->join_failed, 1);
	zassert_equal(atomic_get(&stats->last_join_error), -RZI_ERR_TIMEOUT);
	zassert_equal(fake_join_attempts, 3);
	zassert_equal(fake_leave_calls, 1);
	zassert_equal(atomic_get(&stats->last_state), RZI_LORAWAN_STATE_READY);
	k_sleep(K_MSEC(30));
	zassert_equal(fake_join_attempts, 3);
	zassert_equal(atomic_get(&stats->join_failed), 1);
}

ZTEST(rzi_lorawan_service, test_usp_busy_and_empty_uplink)
{
	struct callback_stats *stats;
	const uint8_t payload[] = {0x12, 0x34};
	bool busy = true;

	ensure_ready_and_left();
	stats = subscribe();
	zassert_equal(rzi_lorawan_send_empty(false, 0, RZI_LORAWAN_MSG_UNCONFIRMED),
		      -RZI_ERR_NOT_JOINED);
	zassert_ok(rzi_lorawan_join(1, 0, RZI_LORAWAN_JOIN_INTERVAL_DEFAULT, 0));
	wait_for_count(&stats->joined, 1);

	fake_tx_hold = true;
	zassert_ok(rzi_lorawan_is_busy(&busy));
	zassert_false(busy);
	zassert_ok(rzi_lorawan_send(10, payload, sizeof(payload), RZI_LORAWAN_MSG_UNCONFIRMED));
	zassert_ok(rzi_lorawan_is_busy(&busy));
	zassert_true(busy);
	zassert_equal(rzi_lorawan_send_empty(true, 20, RZI_LORAWAN_MSG_UNCONFIRMED), -RZI_ERR_BUSY);
	fake_complete_tx();
	wait_for_count(&stats->uplink, 1);
	zassert_ok(rzi_lorawan_is_busy(&busy));
	zassert_false(busy);
	fake_tx_hold = false;

	zassert_equal(rzi_lorawan_send_empty(true, 0, RZI_LORAWAN_MSG_UNCONFIRMED),
		      -RZI_ERR_INVALID);
	zassert_ok(rzi_lorawan_send_empty(true, 20, RZI_LORAWAN_MSG_CONFIRMED));
	wait_for_count(&stats->uplink, 2);
}

ZTEST(rzi_lorawan_service, test_usp_link_controls)
{
	struct callback_stats *stats;
	uint8_t distribution[RZI_LORAWAN_DR_DISTRIBUTION_LENGTH] = {0};
	enum rzi_lorawan_adr_profile profile;
	uint8_t nb_trans = 0;
	int32_t budget;
	bool flag;
	struct rzi_lorawan_csma_params csma;

	ensure_ready_and_left();
	stats = subscribe();
	zassert_equal(rzi_lorawan_set_adr_profile(RZI_LORAWAN_ADR_MOBILE_LOW_POWER, NULL),
		      -RZI_ERR_NOT_JOINED);
	zassert_ok(rzi_lorawan_join(1, 0, RZI_LORAWAN_JOIN_INTERVAL_DEFAULT, 0));
	wait_for_count(&stats->joined, 1);

	zassert_equal(rzi_lorawan_set_adr_profile(RZI_LORAWAN_ADR_CUSTOM, NULL), -RZI_ERR_INVALID);
	distribution[3] = 16;
	zassert_equal(rzi_lorawan_set_adr_profile(RZI_LORAWAN_ADR_CUSTOM, distribution),
		      -RZI_ERR_INVALID);
	zassert_equal(rzi_lorawan_set_adr_profile((enum rzi_lorawan_adr_profile)4, NULL),
		      -RZI_ERR_INVALID);
	zassert_ok(rzi_lorawan_set_adr_profile(RZI_LORAWAN_ADR_NETWORK, NULL));
	zassert_ok(rzi_lorawan_get_adr_profile(&profile));
	zassert_equal(profile, RZI_LORAWAN_ADR_NETWORK);
	zassert_ok(rzi_lorawan_get_adr(&flag));
	zassert_true(flag);

	/* NbTrans belongs to the network while ADR is network-controlled. */
	zassert_equal(rzi_lorawan_set_nb_trans(16), -RZI_ERR_INVALID);
	zassert_equal(rzi_lorawan_set_nb_trans(3), -RZI_ERR_INVALID);
	zassert_ok(rzi_lorawan_set_adr_profile(RZI_LORAWAN_ADR_MOBILE_LONG_RANGE, NULL));
	zassert_ok(rzi_lorawan_set_nb_trans(3));
	zassert_ok(rzi_lorawan_get_nb_trans(&nb_trans));
	zassert_equal(nb_trans, 3);

	/* Controls the fake (like a build without them) does not provide. */
	zassert_equal(rzi_lorawan_set_adr_ack(1, 32), -RZI_ERR_INVALID);
	zassert_equal(rzi_lorawan_set_adr_ack(64, 32), -RZI_ERR_NOT_SUPPORTED);
	zassert_equal(rzi_lorawan_get_duty_cycle_status(&budget), -RZI_ERR_NOT_SUPPORTED);
	zassert_equal(rzi_lorawan_get_csma(&flag), -RZI_ERR_NOT_SUPPORTED);
	zassert_equal(rzi_lorawan_get_csma_params(&csma), -RZI_ERR_NOT_SUPPORTED);
	zassert_equal(rzi_lorawan_set_crystal_error(30), -RZI_ERR_NOT_SUPPORTED);
}

ZTEST(rzi_lorawan_service, test_usp_status_events)
{
	struct callback_stats *stats;
	const struct rzi_lorawan_backend_event limited = {
		.type = RZI_LORAWAN_BACKEND_DUTY_CYCLE,
		.duty_cycle_limited = true,
	};
	const struct rzi_lorawan_backend_event no_downlink = {
		.type = RZI_LORAWAN_BACKEND_NO_DOWNLINK,
		.no_downlink = RZI_LORAWAN_NO_DOWNLINK_USER_THRESHOLD,
	};
	const struct rzi_lorawan_backend_event dropped = {
		.type = RZI_LORAWAN_BACKEND_ERROR,
		.error = -RZI_ERR_OVERFLOW,
	};

	ensure_ready_and_left();
	stats = subscribe();
	fake_sink(&limited);
	fake_sink(&no_downlink);
	fake_sink(&dropped);
	wait_for_count(&stats->errors, 1);
	zassert_equal(atomic_get(&stats->duty_cycle), 1);
	zassert_equal(atomic_get(&stats->no_downlink), RZI_LORAWAN_NO_DOWNLINK_USER_THRESHOLD);
	zassert_equal(atomic_get(&stats->last_error), -RZI_ERR_OVERFLOW);
}

static void assert_all_channels(void)
{
	const uint16_t all[RZI_LORAWAN_CHANNEL_MASK_WORDS] = {0xffff, 0xffff, 0xffff,
							      0xffff, 0x00ff, 0};
	uint16_t mask[RZI_LORAWAN_CHANNEL_MASK_WORDS];

	zassert_ok(rzi_lorawan_get_channel_mask(mask, ARRAY_SIZE(mask)));
	zassert_mem_equal(mask, all, sizeof(all));
}

static void join_and_stop(void)
{
	fake_join_failures = -1;
	zassert_ok(rzi_lorawan_join(1, 0, RZI_LORAWAN_JOIN_INTERVAL_DEFAULT, 3));
	zassert_equal(rzi_lorawan_set_sub_band(1), -RZI_ERR_BUSY);
	zassert_ok(rzi_lorawan_join(0, RZI_LORAWAN_JOIN_KEEP, RZI_LORAWAN_JOIN_KEEP,
				    RZI_LORAWAN_JOIN_KEEP));
	fake_join_failures = 0;
}

ZTEST(rzi_lorawan_service, test_usp_channel_mask_reaches_the_join)
{
	const uint16_t fsb2[RZI_LORAWAN_CHANNEL_MASK_WORDS] = {0xff00, 0, 0, 0, 0x0002, 0};
	uint16_t mask[RZI_LORAWAN_CHANNEL_MASK_WORDS] = {0};
	uint32_t frequency_hz = 1;
	uint8_t sub_band = 9;

	ensure_ready_and_left();
	assert_all_channels();
	zassert_ok(rzi_lorawan_get_sub_band(&sub_band));
	zassert_equal(sub_band, 0);
	zassert_ok(rzi_lorawan_get_fixed_channel(&frequency_hz));
	zassert_equal(frequency_hz, 0);

	zassert_equal(rzi_lorawan_set_channel_mask(mask, ARRAY_SIZE(mask)), -RZI_ERR_INVALID);
	mask[4] = BIT(8); /* channel 72 does not exist */
	zassert_equal(rzi_lorawan_set_channel_mask(mask, ARRAY_SIZE(mask)), -RZI_ERR_INVALID);
	mask[4] = 0x0001;
	zassert_equal(rzi_lorawan_set_channel_mask(mask, 4), -RZI_ERR_INVALID);
	zassert_equal(rzi_lorawan_get_channel_mask(mask, 4), -RZI_ERR_INVALID);
	zassert_equal(rzi_lorawan_set_sub_band(9), -RZI_ERR_INVALID);
	zassert_equal(rzi_lorawan_set_fixed_channel(902400000U), -RZI_ERR_INVALID);

	/* FSB 2: channels 8-15 and 65. */
	zassert_ok(rzi_lorawan_set_sub_band(2));
	zassert_ok(rzi_lorawan_get_channel_mask(mask, ARRAY_SIZE(mask)));
	zassert_mem_equal(mask, fsb2, sizeof(fsb2));
	zassert_ok(rzi_lorawan_get_sub_band(&sub_band));
	zassert_equal(sub_band, 2);
	join_and_stop();
	zassert_true(fake_last_join.channel_mask_set);
	zassert_mem_equal(fake_last_join.channel_mask, fsb2, sizeof(fsb2));

	zassert_ok(rzi_lorawan_set_fixed_channel(902300000U + 10U * 200000U));
	zassert_ok(rzi_lorawan_get_fixed_channel(&frequency_hz));
	zassert_equal(frequency_hz, 904300000U);
	zassert_equal(rzi_lorawan_get_sub_band(&sub_band), -RZI_ERR_NO_DATA);
	zassert_ok(rzi_lorawan_set_fixed_channel(903000000U + 3U * 1600000U));
	zassert_ok(rzi_lorawan_get_channel_mask(mask, ARRAY_SIZE(mask)));
	zassert_equal(mask[0], 0);
	zassert_equal(mask[4], BIT(3));
	zassert_ok(rzi_lorawan_get_fixed_channel(&frequency_hz));
	zassert_equal(frequency_hz, 907800000U);

	/* Zero, or a mask with every channel, lifts the restriction. */
	zassert_ok(rzi_lorawan_set_fixed_channel(0));
	assert_all_channels();
	join_and_stop();
	zassert_false(fake_last_join.channel_mask_set);
	zassert_ok(rzi_lorawan_set_sub_band(5));
	zassert_ok(rzi_lorawan_get_channel_mask(mask, ARRAY_SIZE(mask)));
	memset(mask, 0xff, 8);
	mask[4] = 0x00ff;
	zassert_ok(rzi_lorawan_set_channel_mask(mask, 5));
	join_and_stop();
	zassert_false(fake_last_join.channel_mask_set);
}

ZTEST_SUITE(rzi_lorawan_service, NULL, NULL, NULL, unsubscribe_after, NULL);
