/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LoRaWAN AT package lifecycle, shared state, and command registration.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#if defined(CONFIG_RZI_AT_NVM)
#include <rzi/storage/storage.h>
#endif

#include <rzi/at/at.h>
#include <rzi/lorawan/lorawan.h>

#include "../../at_priv.h"
#include "../at_network_mode.h"
#include "at_lorawan_priv.h"

#define USER_NODE DT_PATH(zephyr_user)
#if DT_NODE_EXISTS(USER_NODE)
#define AT_HAS_DT_DEFAULTS 1
#define REGION_ENUM(name)  DT_CAT(RZI_LORAWAN_REGION_, name)
#define AT_DT_REGION       REGION_ENUM(DT_STRING_UNQUOTED(USER_NODE, user_lorawan_region))
#endif

struct rzi_at_lorawan_context rzi_at_lorawan_context = {
	.region = RZI_LORAWAN_REGION_EU_868,
	.device_class = RZI_LORAWAN_CLASS_A,
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
		return -RZI_ERR_INVALID;
	}
	for (size_t i = 0; i < out_len; ++i) {
		int high = hex_nibble(hex[i * 2U]);
		int low = hex_nibble(hex[i * 2U + 1U]);

		if (high < 0 || low < 0) {
			return -RZI_ERR_INVALID;
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

int rzi_at_lorawan_parse_long(const char *argument, long *value)
{
	char *end = NULL;

	if (argument == NULL || value == NULL) {
		return -RZI_ERR_INVALID;
	}
	*value = strtol(argument, &end, 10);
	if (end == argument || *end != '\0') {
		return -RZI_ERR_INVALID;
	}
	return 0;
}

int rzi_at_lorawan_parse_bool(const char *argument, bool *value)
{
	if (strcmp(argument, "0") == 0) {
		*value = false;
		return 0;
	}
	if (strcmp(argument, "1") == 0) {
		*value = true;
		return 0;
	}
	return -RZI_ERR_INVALID;
}

int rzi_at_lorawan_apply_class(void)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;

	if (!context->service_started || context->device_class == RZI_LORAWAN_CLASS_A) {
		return 0;
	}
	return rzi_lorawan_set_class(context->device_class);
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
		return -RZI_ERR_INVALID;
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

	if (rc == -RZI_ERR_NOT_FOUND) {
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
	uint8_t retries;
	bool confirmed;
	bool found;
	int rc;

	rc = nvm_read("band", &band, sizeof(band), &found);
	if (rc == 0 && found) {
		rc = rzi_at_lorawan_band_to_region(band, &context->region);
	}
	if (rc == 0) {
		rc = nvm_read("cfm", &confirmed, sizeof(confirmed), &found);
	}
	if (rc == 0 && found) {
		atomic_set(&context->confirmed_uplink, confirmed);
	}
	if (rc == 0) {
		rc = nvm_read("rety", &retries, sizeof(retries), &found);
		if (rc == 0 && found && retries <= 7U) {
			context->retries = retries;
		}
	}
	if (rc == 0) {
		rc = nvm_read("netid", &context->net_id, sizeof(context->net_id), &found);
		if (rc == 0 && found) {
			context->net_id_valid = true;
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

static int start_service(void)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	int rc = rzi_lorawan_set_region(context->region);

	if (rc == 0) {
		atomic_set(&context->join_pending, 1);
		rc = rzi_lorawan_start();
	}
	if (rc != 0) {
		atomic_clear(&context->join_pending);
		return rc;
	}
	context->service_started = true;
	return 0;
}

int rzi_at_lorawan_join_start(void)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;

	if (!context->service_started) {
		return start_service();
	}
	if (rzi_at_lorawan_is_joined()) {
		at_event("JOINED");
		return 0;
	}
	return rzi_lorawan_join(1, RZI_LORAWAN_JOIN_KEEP, RZI_LORAWAN_JOIN_KEEP,
				RZI_LORAWAN_JOIN_KEEP);
}

int rzi_at_lorawan_join_stop(void)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;

	atomic_clear(&context->join_pending);
	return context->service_started ? rzi_lorawan_leave() : 0;
}

static void continue_pending_join(void)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;

	if (atomic_cas(&context->join_pending, 1, 0)) {
		if (rzi_at_lorawan_apply_class() != 0 ||
		    rzi_lorawan_join(1, RZI_LORAWAN_JOIN_KEEP, RZI_LORAWAN_JOIN_KEEP,
				     RZI_LORAWAN_JOIN_KEEP) != 0) {
			at_event("JOIN_FAILED_RX_TIMEOUT");
		}
	}
}

static void on_event(const struct rzi_lorawan_event *event, void *user_data)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;

	ARG_UNUSED(user_data);
	switch (event->type) {
	case RZI_LORAWAN_EVENT_READY:
		continue_pending_join();
		break;
	case RZI_LORAWAN_EVENT_STATE_CHANGED:
		if (event->state == RZI_LORAWAN_STATE_READY) {
			continue_pending_join();
		}
		break;
	case RZI_LORAWAN_EVENT_JOINED:
		at_event("JOINED");
		break;
	case RZI_LORAWAN_EVENT_JOIN_FAILED:
		at_event("JOIN_FAILED_RX_TIMEOUT");
		break;
	case RZI_LORAWAN_EVENT_TX_DONE:
		if (!atomic_get(&context->tx_confirmed)) {
			at_event("TX_DONE");
		} else if (event->tx.status == RZI_LORAWAN_TX_ACKED) {
			atomic_set(&context->confirmation_status, 1);
			at_event("SEND_CONFIRMED_OK");
		} else if (event->tx.status == RZI_LORAWAN_TX_NOT_SENT) {
			atomic_clear(&context->confirmation_status);
			at_event("SEND_CONFIRMED_FAILED");
		} else {
			at_event("TX_DONE");
		}
		break;
	case RZI_LORAWAN_EVENT_DOWNLINK: {
		char hex[RZI_LORAWAN_MAX_PAYLOAD * 2U + 1U];

		k_mutex_lock(&context->state_lock, K_FOREVER);
		context->last_downlink.valid = true;
		context->last_downlink.port = event->downlink.port;
		context->last_downlink.size = event->downlink.size;
		memcpy(context->last_downlink.data, event->downlink.data, event->downlink.size);
		k_mutex_unlock(&context->state_lock);
		rzi_at_lorawan_bin_to_hex(event->downlink.data, event->downlink.size, hex);
		at_event("RX_1:%d:%d:UNICAST:%u:%s", event->downlink.rssi_dbm,
			 event->downlink.snr_quarter_db / 4, event->downlink.port, hex);
		break;
	}
	default:
		break;
	}
}

static const struct rzi_lorawan_callbacks callbacks = {
	.on_event = on_event,
};

static int extension_start(void)
{
	struct rzi_at_lorawan_context *context = &rzi_at_lorawan_context;
	int rc = 0;

	k_mutex_init(&context->state_lock);
#if defined(AT_HAS_DT_DEFAULTS)
	{
		static const uint8_t dev_eui[] = DT_PROP(USER_NODE, user_lorawan_device_eui);
		static const uint8_t join_eui[] = DT_PROP(USER_NODE, user_lorawan_join_eui);
		static const uint8_t app_key[] = DT_PROP(USER_NODE, user_lorawan_app_key);
		uint8_t stored[16];

		BUILD_ASSERT(sizeof(dev_eui) == 8);
		BUILD_ASSERT(sizeof(join_eui) == 8);
		BUILD_ASSERT(sizeof(app_key) == 16);
		if (rzi_lorawan_get_dev_eui(stored, sizeof(dev_eui)) == -RZI_ERR_NO_DATA) {
			rc = rzi_lorawan_set_dev_eui(dev_eui, sizeof(dev_eui));
		}
		if (rc == 0 &&
		    rzi_lorawan_get_app_eui(stored, sizeof(join_eui)) == -RZI_ERR_NO_DATA) {
			rc = rzi_lorawan_set_app_eui(join_eui, sizeof(join_eui));
		}
		if (rc == 0 &&
		    rzi_lorawan_get_app_key(stored, sizeof(app_key)) == -RZI_ERR_NO_DATA) {
			rc = rzi_lorawan_set_app_key(app_key, sizeof(app_key));
		}
		context->region = AT_DT_REGION;
		if (rc != 0) {
			return rc;
		}
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
	{
		bool auto_join = false;

		if (rc == 0) {
			rc = rzi_lorawan_get_auto_join(&auto_join);
		}
		if (rc == 0 && auto_join &&
		    rzi_at_network_mode_get() == RZI_AT_NETWORK_MODE_LORAWAN &&
		    rzi_at_lorawan_join_start() != 0) {
			at_event("JOIN_FAILED_RX_TIMEOUT");
		}
	}
	return 0;
}

#if defined(CONFIG_RZI_AT_NVM)
static int delete_stored_key(const char *namespace_name, const char *key)
{
	int rc = rzi_storage_delete(namespace_name, key);

	return rc == 0 || rc == -RZI_ERR_NOT_FOUND ? 0 : rc;
}
#endif

static int extension_factory_reset(void)
{
#if defined(CONFIG_RZI_AT_NVM)
	static const char *const credential_keys[] = {
		"deveui",  "joineui", "appkey",   "genappkey",     "devaddr",       "nwkskey",
		"appskey", "njm",     "autojoin", "join_interval", "join_attempts",
	};
	static const char *const at_keys[] = {
		"deveui",        "joineui",       "appkey", "band", "cfm",     "autojoin",
		"join_interval", "join_attempts", "njm",    "rety", "devaddr", "nwkskey",
		"appskey",       "netid",         "alias",  "sn",   "pword",   "lpm",
	};
	int result = 0;

	for (size_t i = 0; i < ARRAY_SIZE(credential_keys); ++i) {
		int rc = delete_stored_key("lorawan", credential_keys[i]);

		if (rc != 0 && result == 0) {
			result = rc;
		}
	}
	for (size_t i = 0; i < ARRAY_SIZE(at_keys); ++i) {
		int rc = delete_stored_key("rzi", at_keys[i]);

		if (rc != 0 && result == 0) {
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
		&rzi_at_lorawan_supplementary_group,
		&rzi_at_lorawan_information_group,
		&rzi_at_lorawan_class_b_group,
		&rzi_at_lorawan_multicast_group,
		&rzi_at_lorawan_certification_group,
	};

	for (size_t i = 0; i < ARRAY_SIZE(groups); ++i) {
		int rc = rzi_at_register(groups[i]->commands, groups[i]->count);

		if (rc != 0) {
			return rc;
		}
	}
	return rzi_at_registry_add_extension(&extension);
}
