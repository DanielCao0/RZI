/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN AT package lifecycle, shared state, and command registration.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#if defined(CONFIG_RZI_AT_NVM)
#include <rzi/storage.h>
#endif

#include <rzi/at.h>
#include <rzi/lorawan.h>

#include "../../at_priv.h"
#include "at_command_lorawan_priv.h"

#define USER_NODE DT_PATH(zephyr_user)
#if DT_NODE_EXISTS(USER_NODE)
#define AT_HAS_DT_DEFAULTS 1
#define REGION_ENUM(name)  DT_CAT(RZI_LORAWAN_REGION_, name)
#define AT_DT_REGION       REGION_ENUM(DT_STRING_UNQUOTED(USER_NODE, user_lorawan_region))
#endif

struct rzi_at_lorawan_context rzi_at_lorawan_context = {
	.region = RZI_LORAWAN_REGION_EU_868,
	.join_interval = RZI_AT_LORAWAN_JOIN_INTERVAL_DEFAULT,
};

static void ignore_result(int result)
{
	ARG_UNUSED(result);
}

#define at_event(...) ignore_result(rzi_at_publish_event(__VA_ARGS__))

static int hex_nibble(char value)
{
	if (value >= '0' && value <= '9') {
		return value - '0';
	}
	if (value >= 'A' && value <= 'F') {
		return value - 'A' + 10;
	}
	if (value >= 'a' && value <= 'f') {
		return value - 'a' + 10;
	}
	return -1;
}

int rzi_at_lorawan_hex_to_bin(const char *hex, uint8_t *out, size_t out_len)
{
	size_t len = strlen(hex);

	if (len != out_len * 2U) {
		return -EINVAL;
	}
	for (size_t i = 0; i < out_len; ++i) {
		int high = hex_nibble(hex[i * 2U]);
		int low = hex_nibble(hex[i * 2U + 1U]);

		if (high < 0 || low < 0) {
			return -EINVAL;
		}
		out[i] = (uint8_t)((high << 4) | low);
	}
	return 0;
}

void rzi_at_lorawan_bin_to_hex(const uint8_t *in, size_t len, char *out)
{
	static const char digits[] = "0123456789ABCDEF";

	for (size_t i = 0; i < len; ++i) {
		out[i * 2U] = digits[in[i] >> 4];
		out[i * 2U + 1U] = digits[in[i] & 0x0f];
	}
	out[len * 2U] = '\0';
}

int rzi_at_lorawan_band_to_region(int band, enum rzi_lorawan_region *region)
{
	switch (band) {
	case 1:
		*region = RZI_LORAWAN_REGION_CN_470;
		return 0;
	case 2:
		*region = RZI_LORAWAN_REGION_RU_864;
		return 0;
	case 3:
		*region = RZI_LORAWAN_REGION_IN_865;
		return 0;
	case 4:
		*region = RZI_LORAWAN_REGION_EU_868;
		return 0;
	case 5:
		*region = RZI_LORAWAN_REGION_US_915;
		return 0;
	case 6:
		*region = RZI_LORAWAN_REGION_AU_915;
		return 0;
	case 7:
		*region = RZI_LORAWAN_REGION_KR_920;
		return 0;
	case 8:
		*region = RZI_LORAWAN_REGION_AS_923_GRP1;
		return 0;
	case 9:
		*region = RZI_LORAWAN_REGION_AS_923_GRP2;
		return 0;
	case 10:
		*region = RZI_LORAWAN_REGION_AS_923_GRP3;
		return 0;
	case 11:
		*region = RZI_LORAWAN_REGION_AS_923_GRP4;
		return 0;
	default:
		/* 0 = EU433 and 12 = LA915 are unsupported. */
		return -EINVAL;
	}
}

int rzi_at_lorawan_region_to_band(enum rzi_lorawan_region region)
{
	switch (region) {
	case RZI_LORAWAN_REGION_CN_470:
		return 1;
	case RZI_LORAWAN_REGION_RU_864:
		return 2;
	case RZI_LORAWAN_REGION_IN_865:
		return 3;
	case RZI_LORAWAN_REGION_EU_868:
		return 4;
	case RZI_LORAWAN_REGION_US_915:
		return 5;
	case RZI_LORAWAN_REGION_AU_915:
		return 6;
	case RZI_LORAWAN_REGION_KR_920:
		return 7;
	case RZI_LORAWAN_REGION_AS_923_GRP1:
		return 8;
	case RZI_LORAWAN_REGION_AS_923_GRP2:
		return 9;
	case RZI_LORAWAN_REGION_AS_923_GRP3:
		return 10;
	case RZI_LORAWAN_REGION_AS_923_GRP4:
		return 11;
	default:
		return 4;
	}
}

#if defined(CONFIG_RZI_AT_NVM)

static int nvm_read(const char *key, void *dst, size_t len, bool *found)
{
	int rc = rzi_storage_read("rzi", key, dst, len);

	if (rc == -ENOENT) {
		*found = false;
		return 0;
	}
	*found = rc == 0;
	return rc;
}

static int nvm_load(void)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	uint8_t band;
	uint8_t join_value;
	bool confirmed;
	bool found;
	int rc;

	rc = nvm_read("deveui", context->dev_eui, sizeof(context->dev_eui), &found);
	if (rc == 0) {
		rc = nvm_read("joineui", context->join_eui, sizeof(context->join_eui), &found);
	}
	if (rc == 0) {
		rc = nvm_read("appkey", context->app_key, sizeof(context->app_key), &found);
	}
	if (rc == 0) {
		rc = nvm_read("band", &band, sizeof(band), &found);
		if (rc == 0 && found) {
			rc = rzi_at_lorawan_band_to_region(band, &context->region);
		}
	}
	if (rc == 0) {
		rc = nvm_read("cfm", &confirmed, sizeof(confirmed), &found);
	}
	if (rc == 0 && found) {
		atomic_set(&context->confirmed_uplink, confirmed);
	}
	if (rc == 0) {
		rc = nvm_read("autojoin", &context->auto_join, sizeof(context->auto_join), &found);
	}
	if (rc == 0) {
		rc = nvm_read("join_interval", &join_value, sizeof(join_value), &found);
		if (rc == 0 && found) {
			if (join_value < RZI_AT_LORAWAN_JOIN_INTERVAL_MIN) {
				return -EINVAL;
			}
			context->join_interval = join_value;
		}
	}
	if (rc == 0) {
		rc = nvm_read("join_attempts", &join_value, sizeof(join_value), &found);
		if (rc == 0 && found) {
			context->join_attempts = join_value;
		}
	}
	return rc;
}

int rzi_at_lorawan_nvm_save(const char *key, const void *value, size_t len)
{
	return rzi_storage_write("rzi", key, value, len);
}

#else

int rzi_at_lorawan_nvm_save(const char *key, const void *value, size_t len)
{
	ARG_UNUSED(key);
	ARG_UNUSED(value);
	ARG_UNUSED(len);
	return 0;
}

#endif /* CONFIG_RZI_AT_NVM */

bool rzi_at_lorawan_is_joined(void)
{
	bool joined = false;

	if (!rzi_at_lorawan_context.service_started) {
		return false;
	}
	return rzi_lorawan_is_joined(&joined) == 0 && joined;
}

static void prepare_join_config(struct rzi_lorawan_join_config *config)
{
	const struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;

	memset(config, 0, sizeof(*config));
	config->activation = RZI_LORAWAN_ACTIVATION_OTAA;
	memcpy(config->otaa.dev_eui, context->dev_eui, sizeof(config->otaa.dev_eui));
	memcpy(config->otaa.join_eui, context->join_eui, sizeof(config->otaa.join_eui));
	memcpy(config->otaa.network_key, context->app_key, sizeof(config->otaa.network_key));
	memcpy(config->otaa.application_key, context->app_key,
	       sizeof(config->otaa.application_key));
}

static int request_join(void)
{
	struct rzi_lorawan_join_config config;

	prepare_join_config(&config);
	return rzi_lorawan_join(&config);
}

static void report_join_failure(void)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;

	if (atomic_get(&context->join_sequence_active) &&
	    atomic_get(&context->join_retries_remaining) > 0) {
		atomic_dec(&context->join_retries_remaining);
		(void)k_work_reschedule(&context->join_retry_work,
					K_SECONDS(context->join_interval));
		return;
	}

	atomic_clear(&context->join_sequence_active);
	at_event("JOIN_FAILED_RX_TIMEOUT");
}

static void join_retry_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (atomic_get(&rzi_at_lorawan_context.join_sequence_active) && request_join() != 0) {
		report_join_failure();
	}
}

int rzi_at_lorawan_join_start(void)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	int rc;

	if (!atomic_cas(&context->join_sequence_active, 0, 1)) {
		return -EBUSY;
	}
	atomic_set(&context->join_retries_remaining, context->join_attempts);

	if (!context->service_started) {
		rc = rzi_lorawan_set_region(context->region);
		if (rc == 0) {
			prepare_join_config(&context->pending_join_config);
			atomic_set(&context->join_pending, 1);
			rc = rzi_lorawan_start();
		}
		if (rc != 0) {
			atomic_clear(&context->join_pending);
			atomic_clear(&context->join_sequence_active);
			return rc;
		}
		context->service_started = true;
		return 0;
	}

	if (rzi_at_lorawan_is_joined()) {
		atomic_clear(&context->join_sequence_active);
		at_event("JOINED");
		return 0;
	}

	rc = request_join();
	if (rc != 0) {
		atomic_clear(&context->join_sequence_active);
	}
	return rc;
}

int rzi_at_lorawan_join_stop(void)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	struct k_work_sync sync;

	atomic_clear(&context->join_pending);
	atomic_clear(&context->join_sequence_active);
	(void)k_work_cancel_delayable_sync(&context->join_retry_work, &sync);
	return context->service_started ? rzi_lorawan_leave() : 0;
}

static void on_state_changed(enum rzi_lorawan_state state, void *user_data)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;

	ARG_UNUSED(user_data);
	if (state == RZI_LORAWAN_STATE_READY && atomic_cas(&context->join_pending, 1, 0) &&
	    rzi_lorawan_join(&context->pending_join_config) != 0) {
		report_join_failure();
	}
}

static void on_join_done(int status, void *user_data)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;

	ARG_UNUSED(user_data);
	if (status == 0) {
		atomic_clear(&context->join_sequence_active);
		(void)k_work_cancel_delayable(&context->join_retry_work);
		at_event("JOINED");
	} else {
		report_join_failure();
	}
}

static void on_send_done(const struct rzi_lorawan_tx_result *result, void *user_data)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;

	ARG_UNUSED(user_data);
	if (!atomic_get(&context->tx_confirmed)) {
		at_event("TX_DONE");
	} else if (result->status == RZI_LORAWAN_TX_ACKED) {
		atomic_set(&context->confirmation_status, 1);
		at_event("SEND_CONFIRMED_OK");
	} else if (result->status == RZI_LORAWAN_TX_NOT_SENT) {
		atomic_clear(&context->confirmation_status);
		at_event("SEND_CONFIRMED_FAILED");
	} else {
		at_event("TX_DONE");
	}
}

static void on_downlink(const struct rzi_lorawan_downlink *downlink, void *user_data)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	char hex[RZI_LORAWAN_MAX_PAYLOAD * 2U + 1U];

	ARG_UNUSED(user_data);
	k_mutex_lock(&context->state_lock, K_FOREVER);
	context->last_downlink.valid = true;
	context->last_downlink.port = downlink->port;
	context->last_downlink.size = downlink->size;
	memcpy(context->last_downlink.data, downlink->data, downlink->size);
	k_mutex_unlock(&context->state_lock);
	rzi_at_lorawan_bin_to_hex(downlink->data, downlink->size, hex);
	at_event("RX_1:%d:%d:UNICAST:%u:%s", downlink->rssi_dbm, downlink->snr_quarter_db / 4,
		 downlink->port, hex);
}

static const struct rzi_lorawan_callbacks callbacks = {
	.join_done = on_join_done,
	.send_done = on_send_done,
	.downlink = on_downlink,
	.state_changed = on_state_changed,
};

static int extension_start(void)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	int rc;

	k_mutex_init(&context->state_lock);
	k_work_init_delayable(&context->join_retry_work, join_retry_handler);
#if defined(AT_HAS_DT_DEFAULTS)
	BUILD_ASSERT(sizeof(context->dev_eui) == DT_PROP_LEN(USER_NODE, user_lorawan_device_eui));
	BUILD_ASSERT(sizeof(context->join_eui) == DT_PROP_LEN(USER_NODE, user_lorawan_join_eui));
	BUILD_ASSERT(sizeof(context->app_key) == DT_PROP_LEN(USER_NODE, user_lorawan_app_key));
	{
		static const uint8_t dev_eui[] = DT_PROP(USER_NODE, user_lorawan_device_eui);
		static const uint8_t join_eui[] = DT_PROP(USER_NODE, user_lorawan_join_eui);
		static const uint8_t app_key[] = DT_PROP(USER_NODE, user_lorawan_app_key);

		memcpy(context->dev_eui, dev_eui, sizeof(context->dev_eui));
		memcpy(context->join_eui, join_eui, sizeof(context->join_eui));
		memcpy(context->app_key, app_key, sizeof(context->app_key));
		context->region = AT_DT_REGION;
	}
#endif
	rc = rzi_lorawan_register_callbacks(&callbacks, &context->callback_handle);
	if (rc != 0) {
		return rc;
	}
#if defined(CONFIG_RZI_AT_NVM)
	rc = rzi_storage_init();
	if (rc == 0) {
		rc = nvm_load();
	}
	if (rc != 0) {
		return rc;
	}
#endif
	if (context->auto_join && rzi_at_lorawan_join_start() != 0) {
		at_event("JOIN_FAILED_RX_TIMEOUT");
	}
	return 0;
}

static int extension_factory_reset(void)
{
#if defined(CONFIG_RZI_AT_NVM)
	static const char *const keys[] = {"deveui", "joineui",  "appkey",        "band",
					   "cfm",    "autojoin", "join_interval", "join_attempts"};
	int result = 0;

	for (size_t i = 0; i < ARRAY_SIZE(keys); ++i) {
		int rc;

		rc = rzi_storage_delete("rzi", keys[i]);
		if (rc != 0 && rc != -ENOENT && result == 0) {
			result = rc;
		}
	}
	return result;
#else
	return 0;
#endif
}

static const struct rzi_at_extension extension = {
	.start = extension_start,
	.factory_reset = extension_factory_reset,
};

int rzi_at_lorawan_register(void)
{
	static const struct rzi_at_lorawan_command_group *const groups[] = {
		&rzi_at_lorawan_key_id_group,
		&rzi_at_lorawan_join_send_group,
		&rzi_at_lorawan_network_management_group,
	};

	for (size_t i = 0; i < ARRAY_SIZE(groups); ++i) {
		int rc = rzi_at_register(groups[i]->commands, groups[i]->count);

		if (rc != 0) {
			return rc;
		}
	}
	return rzi_at_registry_add_extension(&extension);
}
