/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Central LoRaWAN settings schema, defaults, migration and cache.
 */
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <rzi/lorawan/lorawan.h>
#if defined(CONFIG_RZI_STORAGE)
#include <rzi/storage/storage.h>
#endif
#include "lorawan_settings.h"

#define SETTING_MAX_SIZE 16U

BUILD_ASSERT(RZI_LORAWAN_CHANNEL_MASK_WORDS * sizeof(uint16_t) <= SETTING_MAX_SIZE);

enum value_kind {
	BYTES,
	BOOLEAN,
	RETRIES,
	REGION,
	ACTIVATION,
	NET_ID,
	CHANNEL_MASK,
	INTERVAL,
	JOIN_POLICY
};

struct setting {
	const char *key;
	uint8_t size;
	enum value_kind kind;
	uint8_t default_first;
	bool loaded;
	bool present;
	uint8_t value[SETTING_MAX_SIZE];
};

/* This is also the factory-reset manifest, including all legacy-only keys. */
#define ENTRY(name, bytes, type, first)                                                            \
	{.key = name, .size = bytes, .kind = type, .default_first = first}
static struct setting settings[] = {
	ENTRY("deveui", 8, BYTES, 0),
	ENTRY("joineui", 8, BYTES, 0),
	ENTRY("appkey", 16, BYTES, 0),
	ENTRY("genappkey", 16, BYTES, 0),
	ENTRY("devaddr", 4, BYTES, 0),
	ENTRY("nwkskey", 16, BYTES, 0),
	ENTRY("appskey", 16, BYTES, 0),
	ENTRY("njm", 1, ACTIVATION, 1),
	ENTRY("chmask", RZI_LORAWAN_CHANNEL_MASK_WORDS * 2U, CHANNEL_MASK, 0),
	ENTRY("region", 1, REGION, RZI_LORAWAN_REGION_EU_868),
	ENTRY("cfm", 1, BOOLEAN, 0),
	ENTRY("rety", 1, RETRIES, 0),
	ENTRY("netid", 4, NET_ID, 0),
	ENTRY("join_policy", RZI_JOIN_POLICY_SIZE, JOIN_POLICY, 0),
	ENTRY("autojoin", 1, BOOLEAN, 0),
	ENTRY("join_interval", 1, INTERVAL, RZI_LORAWAN_JOIN_INTERVAL_DEFAULT),
	ENTRY("join_attempts", 1, BYTES, 0),
	ENTRY("band", 1, BYTES, 0),
};
#undef ENTRY

K_MUTEX_DEFINE(schema_lock);

static struct setting *find_setting(const char *key)
{
	for (size_t i = 0; i < ARRAY_SIZE(settings); ++i) {
		if (strcmp(settings[i].key, key) == 0) {
			return &settings[i];
		}
	}
	return NULL;
}

static bool valid_value(const struct setting *setting, const uint8_t *value)
{
	switch (setting->kind) {
	case BOOLEAN:
	case ACTIVATION:
		return value[0] <= 1U;
	case RETRIES:
		return value[0] <= 7U;
	case REGION:
		return value[0] <= RZI_LORAWAN_REGION_RU_864;
	case INTERVAL:
		return value[0] >= RZI_LORAWAN_JOIN_INTERVAL_MIN;
	case NET_ID: {
		uint32_t net_id;

		memcpy(&net_id, value, sizeof(net_id));
		return net_id <= 0xffffffU;
	}
	case CHANNEL_MASK: {
		uint16_t mask[RZI_LORAWAN_CHANNEL_MASK_WORDS];

		memcpy(mask, value, sizeof(mask));
		for (size_t channel = 72U; channel < ARRAY_SIZE(mask) * 16U; ++channel) {
			if ((mask[channel / 16U] & BIT(channel % 16U)) != 0U) {
				return false;
			}
		}
		/* Zero explicitly represents the unrestricted/default channel plan. */
		return true;
	}
	case JOIN_POLICY:
		return value[RZI_JOIN_POLICY_AUTO] <= 1U &&
		       value[RZI_JOIN_POLICY_INTERVAL] >= RZI_LORAWAN_JOIN_INTERVAL_MIN;
	case BYTES:
		return true;
	}
	return false;
}

static void default_value(const struct setting *setting, uint8_t *value)
{
	memset(value, 0, setting->size);
	value[0] = setting->default_first;
	if (setting->kind == JOIN_POLICY) {
		value[RZI_JOIN_POLICY_INTERVAL] = RZI_LORAWAN_JOIN_INTERVAL_DEFAULT;
	}
}

static int save_value(const struct setting *setting, const void *value)
{
#if defined(CONFIG_RZI_STORAGE)
	if (setting->kind == JOIN_POLICY) {
		return rzi_storage_write_record("lorawan", setting->key, RZI_JOIN_POLICY_VERSION,
						value, setting->size);
	}
	return rzi_storage_write("lorawan", setting->key, value, setting->size);
#else
	ARG_UNUSED(setting);
	ARG_UNUSED(value);
	return 0;
#endif
}

#if defined(CONFIG_RZI_STORAGE)
/* Returns a validated candidate, leaving the cache untouched on any failure. */
static int read_legacy(const struct setting *setting, uint8_t *value, bool *found)
{
	int rc = rzi_storage_read("lorawan", setting->key, value, setting->size);

	if (rc == -RZI_ERR_NOT_FOUND) {
		rc = rzi_storage_read("rzi", setting->key, value, setting->size);
	}
	*found = rc == 0;
	if (rc == -RZI_ERR_NOT_FOUND) {
		default_value(setting, value);
		return 0;
	}
	if (rc != 0) {
		return rc;
	}
	return valid_value(setting, value) ? 0 : -RZI_ERR_BAD_MESSAGE;
}

static int migrate_region(uint8_t *value, bool *found)
{
	static const uint8_t bands[] = {
		RZI_LORAWAN_REGION_EU_868,      RZI_LORAWAN_REGION_CN_470,
		RZI_LORAWAN_REGION_RU_864,      RZI_LORAWAN_REGION_IN_865,
		RZI_LORAWAN_REGION_EU_868,      RZI_LORAWAN_REGION_US_915,
		RZI_LORAWAN_REGION_AU_915,      RZI_LORAWAN_REGION_KR_920,
		RZI_LORAWAN_REGION_AS_923_GRP1, RZI_LORAWAN_REGION_AS_923_GRP2,
		RZI_LORAWAN_REGION_AS_923_GRP3, RZI_LORAWAN_REGION_AS_923_GRP4,
	};
	uint8_t band;
	int rc = rzi_storage_read("rzi", "band", &band, sizeof(band));

	*found = rc == 0;
	if (rc == -RZI_ERR_NOT_FOUND) {
		return 0;
	}
	if (rc != 0) {
		return rc;
	}
	if (band == 0U || band >= ARRAY_SIZE(bands)) {
		return -RZI_ERR_BAD_MESSAGE;
	}
	value[0] = bands[band];
	return 0;
}

static int load_value(const struct setting *setting, uint8_t *value, bool *found)
{
	int rc;

	if (setting->kind == JOIN_POLICY) {
		uint16_t version;
		size_t size;

		rc = rzi_storage_read_record("lorawan", setting->key, &version, value,
					     setting->size, &size);
		if (rc == 0) {
			if (version != RZI_JOIN_POLICY_VERSION) {
				return -RZI_ERR_NOT_SUPPORTED;
			}
			if (size != setting->size || !valid_value(setting, value)) {
				return -RZI_ERR_BAD_MESSAGE;
			}
			*found = true;
			return 0;
		}
		if (rc != -RZI_ERR_NOT_FOUND) {
			return rc;
		}
		static const char *const keys[] = {"autojoin", "join_interval", "join_attempts"};

		*found = false;
		for (size_t i = 0; i < ARRAY_SIZE(keys); ++i) {
			bool present;

			rc = read_legacy(find_setting(keys[i]), &value[i], &present);
			if (rc != 0) {
				return rc;
			}
			*found = *found || present;
		}
	} else {
		rc = rzi_storage_read("lorawan", setting->key, value, setting->size);
		if (rc == 0) {
			*found = true;
			return valid_value(setting, value) ? 0 : -RZI_ERR_BAD_MESSAGE;
		}
		if (rc != -RZI_ERR_NOT_FOUND) {
			return rc;
		}
		rc = read_legacy(setting, value, found);
		if (rc != 0) {
			return rc;
		}
		if (!*found && setting->kind == REGION) {
			rc = migrate_region(value, found);
			if (rc != 0) {
				return rc;
			}
		}
	}
	return *found ? save_value(setting, value) : 0;
}
#endif

int rzi_lorawan_setting_read(const char *key, void *value, size_t size, bool *found)
{
	struct setting *setting = find_setting(key);
	uint8_t candidate[SETTING_MAX_SIZE];
	bool present = false;
	int rc = 0;

	if (setting == NULL || size != setting->size) {
		return -RZI_ERR_INVALID;
	}
	k_mutex_lock(&schema_lock, K_FOREVER);
	if (!setting->loaded) {
		default_value(setting, candidate);
#if defined(CONFIG_RZI_STORAGE)
		rc = load_value(setting, candidate, &present);
#endif
		if (rc == 0) {
			memcpy(setting->value, candidate, size);
			setting->present = present;
			setting->loaded = true;
		}
	}
	if (rc == 0) {
		memcpy(value, setting->value, size);
		*found = setting->present;
	}
	k_mutex_unlock(&schema_lock);
	return rc;
}

int rzi_lorawan_setting_write(const char *key, const void *value, size_t size)
{
	struct setting *setting = find_setting(key);
	int rc = 0;

	if (setting == NULL || size != setting->size || !valid_value(setting, value)) {
		return -RZI_ERR_INVALID;
	}
	k_mutex_lock(&schema_lock, K_FOREVER);
	if (!setting->loaded || !setting->present || memcmp(setting->value, value, size) != 0) {
		rc = save_value(setting, value);
	}
	if (rc == 0) {
		memcpy(setting->value, value, size);
		setting->present = true;
		setting->loaded = true;
	}
	k_mutex_unlock(&schema_lock);
	return rc;
}

int rzi_lorawan_settings_clear(void)
{
	int rc = 0;

	k_mutex_lock(&schema_lock, K_FOREVER);
#if defined(CONFIG_RZI_STORAGE)
	static const char *const namespaces[] = {"lorawan", "rzi"};

	for (size_t n = 0; n < ARRAY_SIZE(namespaces); ++n) {
		for (size_t i = 0; i < ARRAY_SIZE(settings); ++i) {
			int one = rzi_storage_delete(namespaces[n], settings[i].key);

			if (one != 0 && one != -RZI_ERR_NOT_FOUND && rc == 0) {
				rc = one;
			}
		}
	}
#endif
	if (rc == 0) {
		for (size_t i = 0; i < ARRAY_SIZE(settings); ++i) {
			memset(settings[i].value, 0, sizeof(settings[i].value));
			settings[i].loaded = false;
			settings[i].present = false;
		}
	}
	k_mutex_unlock(&schema_lock);
	return rc;
}
