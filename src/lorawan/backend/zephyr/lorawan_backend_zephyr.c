/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Zephyr public LoRaWAN API backend for RZI.
 *
 * Zephyr join() and send() block until completion. This adapter copies the
 * request, returns immediately, and finishes the RZI contract on a worker
 * thread so the service event model stays asynchronous. The radio/MAC
 * implementation is loramac-node, not the native or LBM LoRa backends.
 */

#include <errno.h>
#include <limits.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/lorawan/lorawan.h>
#include <zephyr/sys/util.h>

#ifndef CONFIG_LORAWAN_EMUL
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#endif

#ifdef CONFIG_RZI_LORAWAN_FUOTA
#include <zephyr/sys/reboot.h>
#ifdef CONFIG_FLASH_MAP
#include <zephyr/storage/flash_map.h>
#endif
#endif

#include "../lorawan_backend.h"
#include "lorawan_backend_zephyr_priv.h"

BUILD_ASSERT(IS_ENABLED(CONFIG_LORA_MODULE_BACKEND_LORAMAC_NODE),
	     "RZI Zephyr LoRaWAN backend requires loramac-node");

LOG_MODULE_REGISTER(rzi_lw_zephyr, CONFIG_LORAWAN_LOG_LEVEL);

#ifdef CONFIG_RZI_LORAWAN_FUOTA
#define ZEPHYR_CAPABILITIES                                                                        \
	(RZI_LORAWAN_CAP_OTAA | RZI_LORAWAN_CAP_ABP | RZI_LORAWAN_CAP_CLASS_A |                    \
	 RZI_LORAWAN_CAP_CLASS_C | RZI_LORAWAN_CAP_FUOTA | RZI_LORAWAN_CAP_DEVICE_TIME |           \
	 RZI_LORAWAN_CAP_LINK_CHECK | RZI_LORAWAN_CAP_INFORMATION |                                \
	 RZI_LORAWAN_CAP_NETWORK_MANAGEMENT | RZI_LORAWAN_CAP_CHANNEL_MANAGEMENT)
#else
#define ZEPHYR_CAPABILITIES                                                                        \
	(RZI_LORAWAN_CAP_OTAA | RZI_LORAWAN_CAP_ABP | RZI_LORAWAN_CAP_CLASS_A |                    \
	 RZI_LORAWAN_CAP_CLASS_C | RZI_LORAWAN_CAP_DEVICE_TIME | RZI_LORAWAN_CAP_LINK_CHECK |      \
	 RZI_LORAWAN_CAP_INFORMATION | RZI_LORAWAN_CAP_NETWORK_MANAGEMENT |                        \
	 RZI_LORAWAN_CAP_CHANNEL_MANAGEMENT)
#endif

#define WORKER_STACK_SIZE CONFIG_RZI_LORAWAN_ZEPHYR_WORKER_STACK_SIZE
#define WORKER_PRIORITY   CONFIG_RZI_LORAWAN_ZEPHYR_WORKER_PRIORITY
#define CLOCK_POLL_PERIOD K_SECONDS(5)
#define CLOCK_POLL_TRIES  24U

static K_MUTEX_DEFINE(lock);
static K_THREAD_STACK_DEFINE(worker_stack, WORKER_STACK_SIZE);

static struct k_work_q worker_q;
static struct k_work join_work;
static struct k_work send_work;
#ifdef CONFIG_RZI_LORAWAN_FUOTA
#ifdef CONFIG_LORAWAN_APP_CLOCK_SYNC
static struct k_work_delayable clock_poll_work;
static uint8_t clock_poll_left;
#endif
#ifdef CONFIG_LORAWAN_FRAG_TRANSPORT
static bool fuota_services_started;
#endif
#endif

static bool started;
static bool joined;
static bool join_pending;
static bool tx_pending;
static uint16_t next_dev_nonce = 1U;
static enum rzi_lorawan_region current_region = RZI_LORAWAN_REGION_EU_868;
static rzi_lorawan_event_sink_t event_sink;
static struct rzi_lorawan_join_config join_settings;
static uint8_t tx_port;
static uint8_t tx_size;
static enum rzi_lorawan_message_type tx_type;
static uint8_t tx_data[RZI_LORAWAN_MAX_PAYLOAD];

static const enum lorawan_region regions[] = {
	[RZI_LORAWAN_REGION_EU_868] = LORAWAN_REGION_EU868,
	[RZI_LORAWAN_REGION_US_915] = LORAWAN_REGION_US915,
	[RZI_LORAWAN_REGION_AU_915] = LORAWAN_REGION_AU915,
	[RZI_LORAWAN_REGION_CN_470] = LORAWAN_REGION_CN470,
	[RZI_LORAWAN_REGION_AS_923_GRP1] = LORAWAN_REGION_AS923,
	[RZI_LORAWAN_REGION_AS_923_GRP2] = LORAWAN_REGION_AS923,
	[RZI_LORAWAN_REGION_AS_923_GRP3] = LORAWAN_REGION_AS923,
	[RZI_LORAWAN_REGION_AS_923_GRP4] = LORAWAN_REGION_AS923,
	[RZI_LORAWAN_REGION_IN_865] = LORAWAN_REGION_IN865,
	[RZI_LORAWAN_REGION_KR_920] = LORAWAN_REGION_KR920,
	[RZI_LORAWAN_REGION_RU_864] = LORAWAN_REGION_RU864,
};

static int map_region(enum rzi_lorawan_region value, enum lorawan_region *out)
{
	if ((unsigned int)value >= ARRAY_SIZE(regions) || out == NULL) {
		return -RZI_ERR_INVALID;
	}

	*out = regions[value];
	return 0;
}

static void publish(const struct rzi_lorawan_backend_event *event)
{
	event_sink(event);
}

void rzi_lorawan_zephyr_publish(const struct rzi_lorawan_backend_event *event)
{
	publish(event);
}

int rzi_lorawan_zephyr_lock_started(void)
{
	k_mutex_lock(&lock, K_FOREVER);
	if (!started) {
		k_mutex_unlock(&lock);
		return -RZI_ERR_NOT_READY;
	}
	return 0;
}

void rzi_lorawan_zephyr_unlock(void)
{
	k_mutex_unlock(&lock);
}

bool rzi_lorawan_zephyr_busy(void)
{
	return join_pending || tx_pending;
}

enum rzi_lorawan_region rzi_lorawan_zephyr_region(void)
{
	return current_region;
}

uint16_t rzi_lorawan_zephyr_dev_nonce(void)
{
	return next_dev_nonce;
}

void rzi_lorawan_zephyr_set_dev_nonce(uint16_t dev_nonce)
{
	next_dev_nonce = dev_nonce == 0U ? 1U : dev_nonce;
}

static int8_t snr_to_quarter_db(int8_t snr_db)
{
	const int scaled = (int)snr_db * 4;

	if (scaled > INT8_MAX) {
		return INT8_MAX;
	}
	if (scaled < INT8_MIN) {
		return INT8_MIN;
	}
	return (int8_t)scaled;
}

static void on_downlink(uint8_t port, uint8_t flags, int16_t rssi, int8_t snr, uint8_t len,
			const uint8_t *data)
{
	struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_DOWNLINK,
		.downlink.port = port,
		.downlink.rssi_dbm = rssi,
		.downlink.snr_quarter_db = snr_to_quarter_db(snr),
		.downlink.flags = flags,
	};

	if (len > RZI_LORAWAN_MAX_PAYLOAD) {
		len = RZI_LORAWAN_MAX_PAYLOAD;
	}
	event.downlink.size = len;
	if (data != NULL && len > 0U) {
		memcpy(event.downlink.data, data, len);
	}
	publish(&event);

	if ((flags & LORAWAN_TIME_UPDATED) != 0U) {
		struct rzi_lorawan_backend_event time_event = {
			.type = RZI_LORAWAN_BACKEND_DEVICE_TIME,
			.error = 0,
		};

		publish(&time_event);
	}

#ifdef CONFIG_RZI_LORAWAN_FUOTA
	if ((flags & LORAWAN_TIME_UPDATED) != 0U) {
		struct rzi_lorawan_backend_event fuota = {
			.type = RZI_LORAWAN_BACKEND_FUOTA,
			.fuota.kind = RZI_LORAWAN_BACKEND_FUOTA_CLOCK_SYNCED,
			.fuota.successful = true,
		};

		publish(&fuota);
	}
#endif
}

#ifndef CONFIG_LORAWAN_EMUL
static void on_link_check(uint8_t demod_margin, uint8_t nb_gateways)
{
	struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_LINK_CHECK,
		.link_check.demod_margin = demod_margin,
		.link_check.gateway_count = nb_gateways,
	};

	publish(&event);
}
#endif

static struct lorawan_downlink_cb downlink_cb = {
	.port = LW_RECV_PORT_ANY,
	.cb = on_downlink,
};

static int apply_region(enum lorawan_region selected)
{
#ifdef CONFIG_LORAWAN_EMUL
	ARG_UNUSED(selected);
	return 0;
#else
	return lorawan_set_region(selected);
#endif
}

static void fill_zephyr_join(struct lorawan_join_config *out)
{
	memset(out, 0, sizeof(*out));
	if (join_settings.activation == RZI_LORAWAN_ACTIVATION_OTAA) {
		out->mode = LORAWAN_ACT_OTAA;
		out->dev_eui = join_settings.otaa.dev_eui;
		out->otaa.join_eui = join_settings.otaa.join_eui;
		/*
		 * Zephyr uses LoRaWAN 1.1 names. network_key maps to nwk_key
		 * (1.0 AppKey / 1.1 NwkKey). application_key maps to app_key
		 * (1.1 AppKey). On 1.0.x the two RZI keys should be identical
		 * AppKey values; GenAppKey is not a Zephyr join parameter.
		 */
		out->otaa.nwk_key = join_settings.otaa.network_key;
		out->otaa.app_key = join_settings.otaa.application_key;
		if (join_settings.otaa.dev_nonce != 0U) {
			out->otaa.dev_nonce = join_settings.otaa.dev_nonce;
		} else {
			out->otaa.dev_nonce = next_dev_nonce;
			next_dev_nonce++;
		}
	} else {
		out->mode = LORAWAN_ACT_ABP;
		out->abp.dev_addr = join_settings.abp.dev_addr;
		out->abp.nwk_skey = join_settings.abp.network_session_key;
		out->abp.app_skey = join_settings.abp.application_session_key;
	}
}

static void join_work_handler(struct k_work *work)
{
	struct lorawan_join_config config;
	struct rzi_lorawan_backend_event event = {0};
	int rc;

	ARG_UNUSED(work);
	k_mutex_lock(&lock, K_FOREVER);
	fill_zephyr_join(&config);
	k_mutex_unlock(&lock);

	rc = lorawan_join(&config);

	k_mutex_lock(&lock, K_FOREVER);
	join_pending = false;
	joined = rc == 0;
	k_mutex_unlock(&lock);

	if (rc == 0) {
		event.type = RZI_LORAWAN_BACKEND_JOINED;
	} else {
		event.type = RZI_LORAWAN_BACKEND_JOIN_FAILED;
		event.error = rzi_err_from_errno(rc);
	}
	publish(&event);
}

static void send_work_handler(struct k_work *work)
{
	struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_TX_DONE,
	};
	uint8_t port;
	uint8_t size;
	enum rzi_lorawan_message_type type;
	uint8_t payload[RZI_LORAWAN_MAX_PAYLOAD];
	int rc;

	ARG_UNUSED(work);
	k_mutex_lock(&lock, K_FOREVER);
	port = tx_port;
	size = tx_size;
	type = tx_type;
	if (size > 0U) {
		memcpy(payload, tx_data, size);
	}
	k_mutex_unlock(&lock);

	rc = lorawan_send(port, payload, size,
			  type == RZI_LORAWAN_MSG_CONFIRMED ? LORAWAN_MSG_CONFIRMED
							    : LORAWAN_MSG_UNCONFIRMED);

	k_mutex_lock(&lock, K_FOREVER);
	tx_pending = false;
	k_mutex_unlock(&lock);

	if (rc != 0) {
		event.tx_status = RZI_LORAWAN_TX_NOT_SENT;
		event.error = rzi_err_from_errno(rc);
	} else if (type == RZI_LORAWAN_MSG_CONFIRMED) {
		event.tx_status = RZI_LORAWAN_TX_ACKED;
	} else {
		event.tx_status = RZI_LORAWAN_TX_SENT;
	}
	publish(&event);
}

#ifdef CONFIG_RZI_LORAWAN_FUOTA
#ifdef CONFIG_LORAWAN_APP_CLOCK_SYNC
static void clock_poll_handler(struct k_work *work)
{
	uint32_t gps_time;
	struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_FUOTA,
		.fuota.kind = RZI_LORAWAN_BACKEND_FUOTA_CLOCK_SYNCED,
		.fuota.successful = true,
	};

	ARG_UNUSED(work);
	if (lorawan_clock_sync_get(&gps_time) == 0) {
		publish(&event);
		return;
	}
	if (clock_poll_left > 0U) {
		clock_poll_left--;
		k_work_schedule_for_queue(&worker_q, &clock_poll_work, CLOCK_POLL_PERIOD);
	}
}
#endif

#ifdef CONFIG_LORAWAN_FRAG_TRANSPORT
static int frag_descriptor(uint32_t descriptor)
{
	struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_FUOTA,
		.fuota.kind = RZI_LORAWAN_BACKEND_FUOTA_SESSION_STARTED,
		.fuota.successful = true,
	};

	ARG_UNUSED(descriptor);
	publish(&event);
	return 0;
}

static void frag_finished(void)
{
	struct rzi_lorawan_backend_event event = {
		.type = RZI_LORAWAN_BACKEND_FUOTA,
		.fuota.kind = RZI_LORAWAN_BACKEND_FUOTA_TRANSFER_DONE,
		.fuota.successful = true,
	};

	publish(&event);
}
#endif

static int zephyr_fuota_start_clock_sync(void)
{
	int rc = 0;

#ifndef CONFIG_LORAWAN_EMUL
	(void)lorawan_request_device_time(false);
#endif
#ifdef CONFIG_LORAWAN_APP_CLOCK_SYNC
	rc = rzi_err_from_errno(lorawan_clock_sync_run());
	if (rc == 0) {
		clock_poll_left = CLOCK_POLL_TRIES;
		k_work_schedule_for_queue(&worker_q, &clock_poll_work, K_NO_WAIT);
	}
#else
	rc = -RZI_ERR_NOT_SUPPORTED;
#endif
#ifdef CONFIG_LORAWAN_FRAG_TRANSPORT
	if (!fuota_services_started) {
		int frag_rc;

		lorawan_frag_transport_register_descriptor_callback(frag_descriptor);
		frag_rc = lorawan_frag_transport_run(frag_finished);
		if (frag_rc != 0 && rc == 0) {
			rc = rzi_err_from_errno(frag_rc);
		}
		fuota_services_started = true;
	}
#endif
	return rc;
}

static int zephyr_fuota_get_image_size(size_t *size)
{
	ARG_UNUSED(size);
	/*
	 * Zephyr frag_transport does not publish the reconstructed length.
	 * The FUOTA coordinator can recover it from the image header.
	 */
	return -RZI_ERR_NO_DATA;
}

#ifdef CONFIG_FLASH_MAP
#if FIXED_PARTITION_EXISTS(slot1_partition)
#define RZI_ZEPHYR_HAS_SLOT1 1
#endif
#endif

static int zephyr_fuota_read_image(uint32_t offset, uint8_t *buffer, size_t size)
{
#ifdef RZI_ZEPHYR_HAS_SLOT1
	const struct flash_area *area;
	int rc;

	if (buffer == NULL) {
		return -RZI_ERR_INVALID;
	}
	rc = flash_area_open(FIXED_PARTITION_ID(slot1_partition), &area);
	if (rc != 0) {
		return rzi_err_from_errno(rc);
	}
	if (((uint64_t)offset + size) > area->fa_size) {
		flash_area_close(area);
		return -RZI_ERR_INVALID;
	}
	rc = flash_area_read(area, offset, buffer, size);
	flash_area_close(area);
	return rzi_err_from_errno(rc);
#else
	ARG_UNUSED(offset);
	ARG_UNUSED(buffer);
	ARG_UNUSED(size);
	return -RZI_ERR_NOT_SUPPORTED;
#endif
}

static int zephyr_fuota_reboot(void)
{
	sys_reboot(SYS_REBOOT_COLD);
	return -RZI_ERR_IO;
}

static const struct rzi_lorawan_fuota_ops zephyr_fuota_ops = {
	.start_clock_sync = zephyr_fuota_start_clock_sync,
	.get_image_size = zephyr_fuota_get_image_size,
	.read_image = zephyr_fuota_read_image,
	.reboot = zephyr_fuota_reboot,
};

const struct rzi_lorawan_backend_extension rzi_lorawan_zephyr_fuota_extension = {
	.size = sizeof(zephyr_fuota_ops),
	.version = RZI_LORAWAN_FUOTA_OPS_VERSION,
	.api = &zephyr_fuota_ops,
};
#endif

static int zephyr_start(enum rzi_lorawan_region selected_region, bool join_backoff_bypass,
			rzi_lorawan_event_sink_t sink)
{
	enum lorawan_region selected;
	int rc;
	const struct rzi_lorawan_backend_event ready = {
		.type = RZI_LORAWAN_BACKEND_READY,
	};

	if (map_region(selected_region, &selected) != 0 || sink == NULL) {
		return -RZI_ERR_INVALID;
	}
	if (join_backoff_bypass) {
		LOG_DBG("join backoff bypass is ignored by the Zephyr backend");
	}

#ifndef CONFIG_LORAWAN_EMUL
	if (!device_is_ready(DEVICE_DT_GET(DT_ALIAS(lora0)))) {
		return -RZI_ERR_NO_DEVICE;
	}
#endif

	event_sink = sink;
	current_region = selected_region;
	rc = apply_region(selected);
	if (rc != 0) {
		return rzi_err_from_errno(rc);
	}
	rc = lorawan_start();
	if (rc != 0) {
		return rzi_err_from_errno(rc);
	}
	lorawan_register_downlink_callback(&downlink_cb);
#ifndef CONFIG_LORAWAN_EMUL
	lorawan_register_link_check_ans_callback(on_link_check);
#endif

	k_work_queue_init(&worker_q);
	k_work_queue_start(&worker_q, worker_stack, K_THREAD_STACK_SIZEOF(worker_stack),
			   WORKER_PRIORITY, NULL);
	k_thread_name_set(k_work_queue_thread_get(&worker_q), "rzi_lw_z");
	k_work_init(&join_work, join_work_handler);
	k_work_init(&send_work, send_work_handler);
#ifdef CONFIG_RZI_LORAWAN_FUOTA
#ifdef CONFIG_LORAWAN_APP_CLOCK_SYNC
	k_work_init_delayable(&clock_poll_work, clock_poll_handler);
#endif
#endif

	k_mutex_lock(&lock, K_FOREVER);
	started = true;
	joined = false;
	k_mutex_unlock(&lock);
	publish(&ready);
	return 0;
}

static int zephyr_join(const struct rzi_lorawan_join_config *config)
{
	int rc = 0;

	k_mutex_lock(&lock, K_FOREVER);
	if (!started) {
		rc = -RZI_ERR_NOT_READY;
	} else if (join_pending || tx_pending) {
		rc = -RZI_ERR_BUSY;
	} else {
		join_settings = *config;
		join_pending = true;
	}
	k_mutex_unlock(&lock);
	if (rc != 0) {
		return rc;
	}

	k_work_submit_to_queue(&worker_q, &join_work);
	return 0;
}

static int zephyr_leave(void)
{
	/*
	 * Zephyr's public LoRaWAN API has no leave or session-reset call.
	 * Returning -RZI_ERR_NOT_SUPPORTED keeps the RZI session state honest.
	 */
	return -RZI_ERR_NOT_SUPPORTED;
}

static int zephyr_send(uint8_t port, const uint8_t *data, size_t size,
		       enum rzi_lorawan_message_type type)
{
	int rc = 0;

	k_mutex_lock(&lock, K_FOREVER);
	if (!started) {
		rc = -RZI_ERR_NOT_READY;
	} else if (!joined) {
		rc = -RZI_ERR_NOT_JOINED;
	} else if (join_pending || tx_pending) {
		rc = -RZI_ERR_BUSY;
	} else {
		tx_port = port;
		tx_size = (uint8_t)size;
		tx_type = type;
		if (size > 0U) {
			memcpy(tx_data, data, size);
		}
		tx_pending = true;
	}
	k_mutex_unlock(&lock);
	if (rc != 0) {
		return rc;
	}

	k_work_submit_to_queue(&worker_q, &send_work);
	return 0;
}

static int zephyr_set_class(enum rzi_lorawan_class device_class)
{
	enum lorawan_class mapped;
	int rc;

	switch (device_class) {
	case RZI_LORAWAN_CLASS_A:
		mapped = LORAWAN_CLASS_A;
		break;
	case RZI_LORAWAN_CLASS_C:
		mapped = LORAWAN_CLASS_C;
		break;
	case RZI_LORAWAN_CLASS_B:
		return -RZI_ERR_NOT_SUPPORTED;
	default:
		return -RZI_ERR_INVALID;
	}

	rc = lorawan_set_class(mapped);
	if (rc == 0) {
		rzi_lorawan_zephyr_set_class(device_class);
	}
	return rzi_err_from_errno(rc);
}

static int zephyr_is_joined(bool *is_joined)
{
	if (is_joined == NULL) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&lock, K_FOREVER);
	if (!started) {
		k_mutex_unlock(&lock);
		return -RZI_ERR_NOT_READY;
	}
	*is_joined = joined;
	k_mutex_unlock(&lock);
	return 0;
}

const struct rzi_lorawan_backend_api rzi_lorawan_backend = {
	.capabilities = ZEPHYR_CAPABILITIES,
	.start = zephyr_start,
	.join = zephyr_join,
	.leave = zephyr_leave,
	.send = zephyr_send,
	.set_class = zephyr_set_class,
	.is_joined = zephyr_is_joined,
	.get_extension = rzi_lorawan_zephyr_get_extension,
};
