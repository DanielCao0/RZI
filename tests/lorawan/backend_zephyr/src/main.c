/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/lorawan/emul.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/ztest.h>

#include <rzi/lorawan.h>

#include "lorawan_backend.h"

K_SEM_DEFINE(callback_sem, 0, 8);

struct callback_stats {
	atomic_t ready;
	atomic_t joined;
	atomic_t join_status;
	atomic_t uplink;
	atomic_t downlink;
	atomic_t payload_valid;
};

static uint8_t last_uplink_port;
static uint8_t last_uplink_len;
static uint8_t last_uplink[RZI_LORAWAN_MAX_PAYLOAD];

static void on_emul_uplink(uint8_t port, uint8_t len, const uint8_t *data)
{
	last_uplink_port = port;
	last_uplink_len = len;
	if (data != NULL && len > 0U) {
		memcpy(last_uplink, data, len);
	}
}

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

	atomic_set(&stats->join_status, status);
	if (status == 0) {
		atomic_inc(&stats->joined);
	}
	k_sem_give(&callback_sem);
}

static void on_send_done(const struct rzi_lorawan_tx_result *result, void *user_data)
{
	struct callback_stats *stats = user_data;

	zassert_equal(result->status, RZI_LORAWAN_TX_SENT);
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

static void wait_for_callbacks(unsigned int count)
{
	for (unsigned int i = 0; i < count; ++i) {
		zassert_ok(k_sem_take(&callback_sem, K_SECONDS(2)));
	}
}

ZTEST(rzi_lorawan_backend_zephyr, test_async_contract_over_blocking_api)
{
	struct callback_stats stats = {0};
	const struct rzi_lorawan_callbacks callbacks = {
		.join_done = on_join_done,
		.send_done = on_send_done,
		.downlink = on_downlink,
		.state_changed = on_state_changed,
		.user_data = &stats,
	};
	rzi_lorawan_callback_handle_t handle;
	const struct rzi_lorawan_join_config otaa = {
		.activation = RZI_LORAWAN_ACTIVATION_OTAA,
		.otaa.dev_eui = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07},
		.otaa.join_eui = {0},
		.otaa.network_key = {0x11},
		.otaa.application_key = {0x11},
		.otaa.dev_nonce = 7,
	};
	const uint8_t payload[] = {0x12, 0x34};
	const uint8_t downlink[] = {0xab, 0xcd};
	bool joined = false;
	uint32_t caps;

	lorawan_emul_register_uplink_callback(on_emul_uplink);
	zassert_ok(rzi_lorawan_register_callbacks(&callbacks, &handle));
	zassert_ok(rzi_lorawan_set_region(RZI_LORAWAN_REGION_EU_868));
	zassert_ok(rzi_lorawan_set_join_backoff_bypass(true));

	caps = rzi_lorawan_get_capabilities();
	zassert_true((caps & RZI_LORAWAN_CAP_OTAA) != 0U);
	zassert_true((caps & RZI_LORAWAN_CAP_ABP) != 0U);
	zassert_true((caps & RZI_LORAWAN_CAP_CLASS_A) != 0U);
	zassert_true((caps & RZI_LORAWAN_CAP_CLASS_C) != 0U);
	zassert_true((caps & RZI_LORAWAN_CAP_CLASS_B) == 0U);

	zassert_ok(rzi_lorawan_start());
	wait_for_callbacks(1);
	zassert_equal(atomic_get(&stats.ready), 1);

	zassert_ok(rzi_lorawan_join(&otaa));
	wait_for_callbacks(1);
	zassert_equal(atomic_get(&stats.joined), 1);
	zassert_equal(atomic_get(&stats.join_status), 0);
	zassert_ok(rzi_lorawan_is_joined(&joined));
	zassert_true(joined);

	zassert_ok(rzi_lorawan_send(10, payload, sizeof(payload), RZI_LORAWAN_MSG_UNCONFIRMED));
	wait_for_callbacks(1);
	zassert_equal(atomic_get(&stats.uplink), 1);
	zassert_equal(last_uplink_port, 10);
	zassert_equal(last_uplink_len, sizeof(payload));
	zassert_mem_equal(last_uplink, payload, sizeof(payload));

	lorawan_emul_send_downlink(3, false, -80, 6, sizeof(downlink), downlink);
	wait_for_callbacks(1);
	zassert_equal(atomic_get(&stats.downlink), 1);
	zassert_true(atomic_get(&stats.payload_valid));

	zassert_ok(rzi_lorawan_set_class(RZI_LORAWAN_CLASS_A));
	zassert_ok(rzi_lorawan_set_class(RZI_LORAWAN_CLASS_C));
	zassert_equal(rzi_lorawan_set_class(RZI_LORAWAN_CLASS_B), -ENOTSUP);
	zassert_equal(rzi_lorawan_leave(), -ENOTSUP);
	zassert_ok(rzi_lorawan_unregister_callbacks(handle));
}

ZTEST_SUITE(rzi_lorawan_backend_zephyr, NULL, NULL, NULL, NULL, NULL);
