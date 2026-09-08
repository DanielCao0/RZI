/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/ztest.h>

#include <rzi/fuota.h>
#include <rzi/lorawan.h>

#include "lorawan_backend.h"
#include "services/fuota/lorawan_fuota.h"

K_SEM_DEFINE(fuota_sem, 0, 8);

struct fuota_stats {
	atomic_t state;
	atomic_t complete;
};

static rzi_lorawan_event_sink_t fake_sink;
static atomic_t clock_sync_calls;
static uint8_t fake_image[] = {0x11, 0x22, 0x33, 0x44};

static int fake_start(enum rzi_lorawan_region region, bool join_backoff_bypass,
		      rzi_lorawan_event_sink_t event_sink)
{
	const struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_READY,
	};

	ARG_UNUSED(region);
	ARG_UNUSED(join_backoff_bypass);
	fake_sink = event_sink;
	fake_sink(&event);
	return 0;
}

static int fake_join(const struct rzi_lorawan_join_config *config)
{
	const struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_JOINED,
	};

	ARG_UNUSED(config);
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
	ARG_UNUSED(port);
	ARG_UNUSED(data);
	ARG_UNUSED(size);
	ARG_UNUSED(type);
	return 0;
}

static int fake_set_class(enum rzi_lorawan_class device_class)
{
	return device_class == RZI_LORAWAN_CLASS_C ? 0 : -ENOTSUP;
}

static int fake_is_joined(bool *joined)
{
	*joined = true;
	return 0;
}

static int fake_start_clock_sync(void)
{
	atomic_inc(&clock_sync_calls);
	return 0;
}

static int fake_get_image_size(size_t *size)
{
	*size = sizeof(fake_image);
	return 0;
}

static int fake_read_image(uint32_t offset, uint8_t *buffer, size_t size)
{
	if (offset + size > sizeof(fake_image)) {
		return -EINVAL;
	}
	memcpy(buffer, &fake_image[offset], size);
	return 0;
}

static int fake_reboot(void)
{
	return -EIO;
}

static const struct rzi_lorawan_fuota_ops fake_fuota_ops = {
	.start_clock_sync = fake_start_clock_sync,
	.get_image_size = fake_get_image_size,
	.read_image = fake_read_image,
	.reboot = fake_reboot,
};

static const struct rzi_lorawan_backend_extension fake_fuota_extension = {
	.size = sizeof(fake_fuota_ops),
	.version = RZI_LORAWAN_FUOTA_OPS_VERSION,
	.api = &fake_fuota_ops,
};

static const struct rzi_lorawan_backend_extension *
fake_get_extension(enum rzi_lorawan_feature_id feature)
{
	return feature == RZI_LORAWAN_FEATURE_FUOTA ? &fake_fuota_extension : NULL;
}

const struct rzi_lorawan_backend_api rzi_lorawan_backend = {
	.capabilities = RZI_LORAWAN_CAP_OTAA | RZI_LORAWAN_CAP_CLASS_A | RZI_LORAWAN_CAP_CLASS_C |
			RZI_LORAWAN_CAP_FUOTA,
	.start = fake_start,
	.join = fake_join,
	.leave = fake_leave,
	.send = fake_send,
	.set_class = fake_set_class,
	.is_joined = fake_is_joined,
	.get_extension = fake_get_extension,
};

static void on_state_changed(enum rzi_fuota_state next, void *user_data)
{
	struct fuota_stats *stats = user_data;

	atomic_set(&stats->state, next);
	k_sem_give(&fuota_sem);
}

static void on_complete(int status, void *user_data)
{
	struct fuota_stats *stats = user_data;

	atomic_set(&stats->complete, status);
	k_sem_give(&fuota_sem);
}

static void wait_for(unsigned int count)
{
	for (unsigned int i = 0; i < count; ++i) {
		zassert_ok(k_sem_take(&fuota_sem, K_SECONDS(1)));
	}
}

ZTEST(rzi_lorawan_fuota, test_session_and_image_access)
{
	struct fuota_stats stats = {0};
	const struct rzi_fuota_callbacks fuota_callbacks = {
		.state_changed = on_state_changed,
		.complete = on_complete,
		.user_data = &stats,
	};
	const struct rzi_lorawan_join_config otaa = {
		.activation = RZI_LORAWAN_ACTIVATION_OTAA,
	};
	struct rzi_fuota_status status = {0};
	uint8_t image[sizeof(fake_image)] = {0};
	const struct rzi_lorawan_backend_event started = {
		.type = RZI_LORAWAN_BACKEND_FUOTA,
		.fuota.kind = RZI_LORAWAN_BACKEND_FUOTA_SESSION_STARTED,
		.fuota.successful = true,
	};
	const struct rzi_lorawan_backend_event done = {
		.type = RZI_LORAWAN_BACKEND_FUOTA,
		.fuota.kind = RZI_LORAWAN_BACKEND_FUOTA_TRANSFER_DONE,
		.fuota.successful = true,
	};

	zassert_ok(rzi_fuota_register_callbacks(&fuota_callbacks));
	zassert_ok(rzi_lorawan_set_region(RZI_LORAWAN_REGION_EU_868));
	zassert_ok(rzi_lorawan_start());
	zassert_true((rzi_lorawan_get_capabilities() & RZI_LORAWAN_CAP_FUOTA) != 0U);
	zassert_ok(rzi_lorawan_join(&otaa));
	wait_for(1);
	zassert_equal(atomic_get(&clock_sync_calls), 1);
	zassert_equal(atomic_get(&stats.state), RZI_FUOTA_STATE_READY);
	zassert_ok(rzi_fuota_get_status(&status));
	zassert_equal(status.state, RZI_FUOTA_STATE_READY);
	zassert_false(status.image_ready);

	fake_sink(&started);
	wait_for(1);
	zassert_equal(atomic_get(&stats.state), RZI_FUOTA_STATE_TRANSFERRING);
	fake_sink(&done);
	wait_for(2);
	zassert_equal(atomic_get(&stats.state), RZI_FUOTA_STATE_COMPLETE);
	zassert_ok(atomic_get(&stats.complete));
	zassert_true(rzi_lorawan_fuota_has_image());
	zassert_ok(rzi_fuota_get_status(&status));
	zassert_true(status.image_ready);
	zassert_equal(status.image_size, sizeof(fake_image));
	zassert_ok(rzi_fuota_read_image(0, image, sizeof(image)));
	zassert_mem_equal(image, fake_image, sizeof(fake_image));
	zassert_ok(rzi_lorawan_set_class(RZI_LORAWAN_CLASS_C));
	zassert_equal(rzi_fuota_apply(), -ENOTSUP);
}

ZTEST_SUITE(rzi_lorawan_fuota, NULL, NULL, NULL, NULL, NULL);
