/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/ztest.h>

#include <rzi/lorawan.h>

#include "lorawan_backend.h"

K_SEM_DEFINE(callback_sem, 0, 8);

struct callback_stats {
	atomic_t ready;
	atomic_t joined;
	atomic_t uplink;
	atomic_t downlink;
	atomic_t errors;
	atomic_t payload_valid;
};

static rzi_lorawan_event_sink_t fake_sink;

static int fake_start(enum rzi_lorawan_region region, bool join_backoff_bypass,
		      rzi_lorawan_event_sink_t event_sink)
{
	const struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_READY,
	};

	zassert_equal(region, RZI_LORAWAN_REGION_US_915);
	zassert_true(join_backoff_bypass);
	fake_sink = event_sink;
	fake_sink(&event);
	return 0;
}

static int fake_join(const struct rzi_lorawan_join_config *config)
{
	const struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_JOINED,
	};

	zassert_equal(config->activation, RZI_LORAWAN_ACTIVATION_OTAA);
	fake_sink(&event);
	return 0;
}

static int fake_leave(void)
{
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

static int fake_set_class(enum rzi_lorawan_class device_class)
{
	return device_class == RZI_LORAWAN_CLASS_A ? 0 : -ENOTSUP;
}

static int fake_is_joined(bool *joined)
{
	*joined = true;
	return 0;
}

const struct rzi_lorawan_backend_api rzi_lorawan_backend = {
	.capabilities = RZI_LORAWAN_CAP_OTAA | RZI_LORAWAN_CAP_CLASS_A,
	.start = fake_start,
	.join = fake_join,
	.leave = fake_leave,
	.send = fake_send,
	.set_class = fake_set_class,
	.is_joined = fake_is_joined,
};

static void on_state_changed(enum rzi_lorawan_state state, void *user_data)
{
	struct callback_stats *stats = user_data;

	if (state == RZI_LORAWAN_STATE_READY) {
		atomic_inc(&stats->ready);
		k_sem_give(&callback_sem);
	}
}

static void on_join_done(int status, void *user_data)
{
	struct callback_stats *stats = user_data;

	zassert_ok(status);
	atomic_inc(&stats->joined);
	k_sem_give(&callback_sem);
}

static void on_send_done(const struct rzi_lorawan_tx_result *result, void *user_data)
{
	struct callback_stats *stats = user_data;

	zassert_equal(result->status, RZI_LORAWAN_TX_ACKED);
	zassert_ok(result->error);
	atomic_inc(&stats->uplink);
	k_sem_give(&callback_sem);
}

static void on_downlink(const struct rzi_lorawan_downlink *downlink, void *user_data)
{
	struct callback_stats *stats = user_data;

	if (downlink->port == 3 && downlink->size == 2 && downlink->data[0] == 0xab &&
	    downlink->data[1] == 0xcd) {
		atomic_set(&stats->payload_valid, 1);
	}
	atomic_inc(&stats->downlink);
	k_sem_give(&callback_sem);
}

static void on_error(int error, void *user_data)
{
	struct callback_stats *stats = user_data;

	zassert_equal(error, -EIO);
	atomic_inc(&stats->errors);
	k_sem_give(&callback_sem);
}

static const struct rzi_lorawan_callbacks callbacks = {
	.join_done = on_join_done,
	.send_done = on_send_done,
	.downlink = on_downlink,
	.state_changed = on_state_changed,
	.error = on_error,
};

static void wait_for_callbacks(unsigned int count)
{
	for (unsigned int i = 0; i < count; ++i) {
		zassert_ok(k_sem_take(&callback_sem, K_SECONDS(1)));
	}
}

ZTEST(rzi_lorawan_core, test_lifecycle_and_multiple_subscribers)
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
	zassert_equal(rzi_lorawan_register_callbacks(&callbacks, &second_handle), -ENOMEM);

	zassert_ok(rzi_lorawan_set_region(RZI_LORAWAN_REGION_US_915));
	zassert_ok(rzi_lorawan_set_join_backoff_bypass(true));
	zassert_ok(rzi_lorawan_start());
	wait_for_callbacks(2);
	zassert_equal(atomic_get(&first.ready), 1);
	zassert_equal(atomic_get(&second.ready), 1);
	zassert_equal(rzi_lorawan_start(), -EALREADY);

	zassert_equal(rzi_lorawan_join(&abp), -ENOTSUP);
	zassert_ok(rzi_lorawan_join(&otaa));
	wait_for_callbacks(2);
	zassert_equal(atomic_get(&first.joined), 1);
	zassert_equal(atomic_get(&second.joined), 1);

	zassert_ok(rzi_lorawan_unregister_callbacks(first_handle));
	zassert_ok(rzi_lorawan_send(10, payload, sizeof(payload), RZI_LORAWAN_MSG_CONFIRMED));
	wait_for_callbacks(1);
	zassert_equal(atomic_get(&first.uplink), 0);
	zassert_equal(atomic_get(&second.uplink), 1);

	{
		const struct rzi_lorawan_backend_event downlink = {
			.type = RZI_LORAWAN_BACKEND_DOWNLINK,
			.downlink.port = 3,
			.downlink.size = 2,
			.downlink.data = {0xab, 0xcd},
		};
		const struct rzi_lorawan_backend_event error = {
			.type = RZI_LORAWAN_BACKEND_ERROR,
			.error = -EIO,
		};

		fake_sink(&downlink);
		wait_for_callbacks(1);
		fake_sink(&error);
		wait_for_callbacks(1);
	}
	zassert_equal(atomic_get(&second.downlink), 1);
	zassert_true(atomic_get(&second.payload_valid));
	zassert_equal(atomic_get(&second.errors), 1);

	zassert_ok(rzi_lorawan_set_class(RZI_LORAWAN_CLASS_A));
	zassert_equal(rzi_lorawan_set_class(RZI_LORAWAN_CLASS_C), -ENOTSUP);
	zassert_ok(rzi_lorawan_is_joined(&joined));
	zassert_true(joined);
	zassert_ok(rzi_lorawan_unregister_callbacks(second_handle));
	zassert_equal(rzi_lorawan_unregister_callbacks(second_handle), -ENOENT);
}

ZTEST_SUITE(rzi_lorawan_core, NULL, NULL, NULL, NULL, NULL);
