/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Backend-independent LoRaWAN FUOTA coordination.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <rzi/lorawan/fuota.h>
#include <rzi/lorawan/lorawan.h>

#if defined(CONFIG_RZI_POWER) && defined(CONFIG_RZI_POWER_AUTO_SERVICE_BLOCK)
#include <rzi/power/power.h>
#endif

#include "../../backend/lorawan_backend.h"
#include "../../lorawan_service.h"

static __maybe_unused void ignore_result(int result)
{
	ARG_UNUSED(result);
}

#ifdef CONFIG_IMG_MANAGER
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#endif

LOG_MODULE_REGISTER(rzi_fuota, LOG_LEVEL_INF);

#define IMAGE_MAGIC     0x96f3b83dU
#define IMAGE_TLV_MAGIC 0x6907U
#define IMAGE_HDR_SIZE  32U
#define IMAGE_HEAD_LOG  16U
#define APPLY_CHUNK     256U

static K_MUTEX_DEFINE(lock);
static atomic_t started;
static enum rzi_fuota_state state = RZI_FUOTA_STATE_IDLE;
static bool image_ready;
#if defined(CONFIG_RZI_POWER) && defined(CONFIG_RZI_POWER_AUTO_SERVICE_BLOCK)
static bool power_flash_held;
#endif
static size_t image_size;
static size_t expected_size;
static int last_error;
static struct rzi_fuota_callbacks callbacks;
static const struct rzi_lorawan_fuota_ops *ops;

#ifdef CONFIG_RZI_LORAWAN_FUOTA_AUTO_APPLY
static void apply_work_handler(struct k_work *work);
static K_WORK_DEFINE(apply_work, apply_work_handler);
#endif

#if CONFIG_RZI_LORAWAN_FUOTA_KEEPALIVE_INTERVAL_S > 0
static void keepalive_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(keepalive_work, keepalive_work_handler);
#endif

static bool callbacks_empty(const struct rzi_fuota_callbacks *table)
{
	return table->state_changed == NULL && table->session_started == NULL &&
	       table->complete == NULL;
}

static const struct rzi_lorawan_fuota_ops *resolve_ops(void)
{
	return rzi_lorawan_backend.fuota;
}

static void set_state_locked(enum rzi_fuota_state next)
{
	struct rzi_fuota_callbacks table = callbacks;

	if (state == next) {
		return;
	}
	state = next;
#if defined(CONFIG_RZI_POWER) && defined(CONFIG_RZI_POWER_AUTO_SERVICE_BLOCK)
	{
		bool want =
			next == RZI_FUOTA_STATE_TRANSFERRING || next == RZI_FUOTA_STATE_APPLYING;

		if (want && !power_flash_held && rzi_power_block(RZI_POWER_BLOCK_FLASH) == 0) {
			power_flash_held = true;
		} else if (!want && power_flash_held) {
			ignore_result(rzi_power_unblock(RZI_POWER_BLOCK_FLASH));
			power_flash_held = false;
		}
	}
#endif
	k_mutex_unlock(&lock);
	if (table.state_changed != NULL) {
		table.state_changed(next, table.user_data);
	}
	k_mutex_lock(&lock, K_FOREVER);
}

static void notify_complete(int status)
{
	struct rzi_fuota_callbacks table = callbacks;

	k_mutex_unlock(&lock);
	if (table.complete != NULL) {
		table.complete(status, table.user_data);
	}
	k_mutex_lock(&lock, K_FOREVER);
}

static void notify_session_started(void)
{
	struct rzi_fuota_callbacks table = callbacks;

	k_mutex_unlock(&lock);
	if (table.session_started != NULL) {
		table.session_started(table.user_data);
	}
	k_mutex_lock(&lock, K_FOREVER);
}

#if CONFIG_RZI_LORAWAN_FUOTA_KEEPALIVE_INTERVAL_S > 0
static void schedule_keepalive(void)
{
	k_work_schedule(&keepalive_work, K_SECONDS(CONFIG_RZI_LORAWAN_FUOTA_KEEPALIVE_INTERVAL_S));
}

static void cancel_keepalive(void)
{
	(void)k_work_cancel_delayable(&keepalive_work);
}

static void keepalive_work_handler(struct k_work *work)
{
	static const uint8_t payload[] = {0x00};
	int rc;

	ARG_UNUSED(work);
	if (!atomic_get(&started) || state == RZI_FUOTA_STATE_TRANSFERRING ||
	    state == RZI_FUOTA_STATE_APPLYING) {
		return;
	}
	rc = rzi_lorawan_send(CONFIG_RZI_LORAWAN_FUOTA_KEEPALIVE_PORT, payload, sizeof(payload),
			      RZI_LORAWAN_MSG_UNCONFIRMED);
	if (rc != 0 && rc != -RZI_ERR_BUSY && rc != -RZI_ERR_NOT_READY) {
		LOG_WRN("FUOTA keepalive rejected: %d", rc);
	}
	schedule_keepalive();
}
#else
static void schedule_keepalive(void)
{
}

static void cancel_keepalive(void)
{
}
#endif

static void log_image_head(void)
{
	uint8_t prefix[IMAGE_HEAD_LOG];
	size_t request = sizeof(prefix);
	int rc;

	if (ops == NULL || ops->read_image == NULL) {
		return;
	}
	if (image_size != 0U && image_size < request) {
		request = image_size;
	}
	rc = ops->read_image(0U, prefix, request);
	if (rc != 0) {
		LOG_WRN("FUOTA image head unread: %d", rc);
		return;
	}
	LOG_HEXDUMP_INF(prefix, request, "FUOTA image head");
}

static int detect_image_size(size_t reported)
{
	uint8_t header[IMAGE_HDR_SIZE];
	uint8_t tlv[4];
	uint32_t magic;
	uint16_t hdr_size;
	uint32_t payload_size;
	int rc;

	if (expected_size != 0U) {
		image_size = expected_size;
		return 0;
	}
	if (reported != 0U) {
		image_size = reported;
		return 0;
	}
	if (ops != NULL && ops->get_image_size != NULL) {
		size_t measured = 0U;

		rc = ops->get_image_size(&measured);
		if (rc == 0 && measured != 0U) {
			image_size = measured;
			return 0;
		}
	}
	if (ops == NULL || ops->read_image == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	rc = ops->read_image(0U, header, sizeof(header));
	if (rc != 0) {
		return rc;
	}
	magic = sys_get_le32(&header[0]);
	if (magic != IMAGE_MAGIC) {
		return -RZI_ERR_NO_DATA;
	}
	hdr_size = sys_get_le16(&header[8]);
	payload_size = sys_get_le32(&header[12]);
	rc = ops->read_image(hdr_size + payload_size, tlv, sizeof(tlv));
	if (rc != 0) {
		return rc;
	}
	if (sys_get_le16(&tlv[0]) != IMAGE_TLV_MAGIC) {
		image_size = (size_t)hdr_size + payload_size;
		return 0;
	}
	image_size = (size_t)hdr_size + payload_size + sys_get_le16(&tlv[2]);
	return 0;
}

#ifdef CONFIG_IMG_MANAGER
static int copy_image_to_slot(void)
{
	struct flash_img_context ctx;
	uint8_t chunk[APPLY_CHUNK];
	size_t remaining = image_size;
	size_t offset = 0U;
	int rc;

	if (ops == NULL || ops->read_image == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	rc = flash_img_init(&ctx);
	if (rc != 0) {
		return rzi_err_from_errno(rc);
	}
	while (remaining > 0U) {
		size_t request = MIN(remaining, sizeof(chunk));
		bool flush = remaining <= sizeof(chunk);

		rc = ops->read_image((uint32_t)offset, chunk, request);
		if (rc != 0) {
			return rc;
		}
		rc = flash_img_buffered_write(&ctx, chunk, request, flush);
		if (rc != 0) {
			return rzi_err_from_errno(rc);
		}
		offset += request;
		remaining -= request;
	}
	return rzi_err_from_errno(boot_request_upgrade(BOOT_UPGRADE_TEST));
}
#endif

#ifdef CONFIG_RZI_LORAWAN_FUOTA_AUTO_APPLY
static void apply_work_handler(struct k_work *work)
{
	int rc;

	ARG_UNUSED(work);
	rc = rzi_fuota_apply();
	if (rc != 0) {
		LOG_ERR("Automatic FUOTA apply failed: %d", rc);
	}
}
#endif

static void handle_joined(void)
{
	int rc = 0;

	image_ready = false;
	image_size = expected_size;
	last_error = 0;
	if (ops != NULL && ops->start_clock_sync != NULL) {
		rc = ops->start_clock_sync();
	}
	if (rc != 0) {
		last_error = rc;
		LOG_WRN("FUOTA clock sync failed: %d", rc);
	}
	set_state_locked(RZI_FUOTA_STATE_READY);
	schedule_keepalive();
}

static void handle_fuota_event(const struct rzi_lorawan_backend_event *event)
{
	switch (event->fuota.kind) {
	case RZI_LORAWAN_BACKEND_FUOTA_CLOCK_SYNCED:
		LOG_INF("FUOTA clock synchronized");
		break;
	case RZI_LORAWAN_BACKEND_FUOTA_SESSION_STARTED:
		cancel_keepalive();
		set_state_locked(RZI_FUOTA_STATE_TRANSFERRING);
		notify_session_started();
		break;
	case RZI_LORAWAN_BACKEND_FUOTA_SESSION_ENDED:
		if (state == RZI_FUOTA_STATE_TRANSFERRING && !image_ready) {
			schedule_keepalive();
			set_state_locked(RZI_FUOTA_STATE_READY);
		}
		break;
	case RZI_LORAWAN_BACKEND_FUOTA_TRANSFER_DONE:
		if (event->fuota.successful) {
			int rc = detect_image_size(event->fuota.image_size);

			image_ready = true;
			last_error = rc == -RZI_ERR_NO_DATA ? 0 : rc;
			if (rc == 0) {
				LOG_INF("FUOTA image ready, %u bytes", (unsigned int)image_size);
			} else {
				LOG_INF("FUOTA image ready, size unknown");
			}
			log_image_head();
			set_state_locked(RZI_FUOTA_STATE_COMPLETE);
			notify_complete(0);
#ifdef CONFIG_RZI_LORAWAN_FUOTA_AUTO_APPLY
			k_work_submit(&apply_work);
#endif
		} else {
			image_ready = false;
			last_error = -RZI_ERR_IO;
			set_state_locked(RZI_FUOTA_STATE_FAILED);
			notify_complete(-RZI_ERR_IO);
			schedule_keepalive();
		}
		break;
	case RZI_LORAWAN_BACKEND_FUOTA_REBOOT_REQUESTED:
		if (image_ready) {
#ifdef CONFIG_RZI_LORAWAN_FUOTA_AUTO_APPLY
			k_work_submit(&apply_work);
#else
			LOG_INF("FMP reboot requested; call rzi_fuota_apply()");
#endif
		}
		break;
	}
}

int rzi_fuota_register_callbacks(const struct rzi_fuota_callbacks *table)
{
	if (k_is_in_isr()) {
		return -RZI_ERR_WOULDBLOCK;
	}
	if (table == NULL || callbacks_empty(table)) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&lock, K_FOREVER);
	callbacks = *table;
	k_mutex_unlock(&lock);
	return 0;
}

int rzi_fuota_start(void)
{
	if (k_is_in_isr()) {
		return -RZI_ERR_WOULDBLOCK;
	}

	k_mutex_lock(&lock, K_FOREVER);
	if (atomic_get(&started)) {
		k_mutex_unlock(&lock);
		return 0;
	}
	ops = resolve_ops();
	if (ops == NULL && (rzi_lorawan_get_capabilities() & RZI_LORAWAN_CAP_FUOTA) == 0U) {
		k_mutex_unlock(&lock);
		return -RZI_ERR_NOT_SUPPORTED;
	}
	atomic_set(&started, 1);
	k_mutex_unlock(&lock);
	LOG_INF("FUOTA coordination started");
	return 0;
}

int rzi_fuota_get_status(struct rzi_fuota_status *status)
{
	if (k_is_in_isr()) {
		return -RZI_ERR_WOULDBLOCK;
	}
	if (status == NULL) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&lock, K_FOREVER);
	status->state = state;
	status->image_ready = image_ready;
	status->image_size = image_size;
	status->last_error = last_error;
	k_mutex_unlock(&lock);
	return 0;
}

int rzi_fuota_set_expected_size(size_t size)
{
	if (k_is_in_isr()) {
		return -RZI_ERR_WOULDBLOCK;
	}
	if (size == 0U) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&lock, K_FOREVER);
	expected_size = size;
	if (image_ready && image_size == 0U) {
		image_size = size;
	}
	k_mutex_unlock(&lock);
	return 0;
}

int rzi_fuota_read_image(size_t offset, void *buffer, size_t size)
{
	int rc;

	if (k_is_in_isr()) {
		return -RZI_ERR_WOULDBLOCK;
	}
	if (buffer == NULL || size == 0U) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&lock, K_FOREVER);
	if (!image_ready) {
		rc = -RZI_ERR_NOT_FOUND;
	} else if (ops == NULL || ops->read_image == NULL) {
		rc = -RZI_ERR_NOT_SUPPORTED;
	} else {
		rc = ops->read_image((uint32_t)offset, buffer, size);
	}
	k_mutex_unlock(&lock);
	return rc;
}

int rzi_fuota_apply(void)
{
	if (k_is_in_isr()) {
		return -RZI_ERR_WOULDBLOCK;
	}

	k_mutex_lock(&lock, K_FOREVER);
	if (!image_ready) {
		k_mutex_unlock(&lock);
		return -RZI_ERR_NOT_FOUND;
	}
	if (image_size == 0U) {
		k_mutex_unlock(&lock);
		return -RZI_ERR_NO_DATA;
	}
	set_state_locked(RZI_FUOTA_STATE_APPLYING);
	k_mutex_unlock(&lock);

#ifndef CONFIG_IMG_MANAGER
	ARG_UNUSED(ops);
	return -RZI_ERR_NOT_SUPPORTED;
#else
	int rc = copy_image_to_slot();

	if (rc != 0) {
		k_mutex_lock(&lock, K_FOREVER);
		last_error = rc;
		set_state_locked(RZI_FUOTA_STATE_COMPLETE);
		k_mutex_unlock(&lock);
		return rc;
	}
	if (ops != NULL && ops->reboot != NULL) {
		return ops->reboot();
	}
	return -RZI_ERR_NOT_SUPPORTED;
#endif
}

static void fuota_on_event(const struct rzi_lorawan_backend_event *event)
{
	if (event == NULL || !atomic_get(&started)) {
		return;
	}

	k_mutex_lock(&lock, K_FOREVER);
	switch (event->type) {
	case RZI_LORAWAN_BACKEND_JOINED:
		handle_joined();
		break;
	case RZI_LORAWAN_BACKEND_JOIN_FAILED:
		cancel_keepalive();
		image_ready = false;
		set_state_locked(RZI_FUOTA_STATE_IDLE);
		break;
	case RZI_LORAWAN_BACKEND_STATE_CHANGED:
		if (event->state == RZI_LORAWAN_STATE_READY && state != RZI_FUOTA_STATE_IDLE) {
			cancel_keepalive();
			image_ready = false;
			set_state_locked(RZI_FUOTA_STATE_IDLE);
		}
		break;
	case RZI_LORAWAN_BACKEND_FUOTA:
		handle_fuota_event(event);
		break;
	default:
		break;
	}
	k_mutex_unlock(&lock);
}

static void fuota_on_started(void)
{
	ignore_result(rzi_fuota_start());
}

static int fuota_register(void)
{
	static const struct rzi_lorawan_service service = {
		.on_started = fuota_on_started,
		.on_event = fuota_on_event,
	};

	return rzi_lorawan_register_service(&service);
}

SYS_INIT(fuota_register, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
