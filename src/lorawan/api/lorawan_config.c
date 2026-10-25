/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Shared LoRaWAN configuration and legacy AT settings migration.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <rzi/lorawan/lorawan.h>
#if defined(CONFIG_RZI_STORAGE)
#include <rzi/storage/storage.h>
#endif
#include "../lorawan_priv.h"

K_MUTEX_DEFINE(settings_lock);
static bool confirmed;
static uint8_t retries;
static uint32_t configured_net_id;
static bool net_id_set;

/* Read through legacy keys only when the service key does not exist. */
static int read_setting(const char *key, void *value, size_t size)
{
#if defined(CONFIG_RZI_STORAGE)
	int rc = rzi_storage_read("lorawan", key, value, size);

	if (rc != -RZI_ERR_NOT_FOUND) {
		return rc;
	}
	rc = rzi_storage_read("rzi", key, value, size);
	if (rc == 0) {
		rc = rzi_storage_write("lorawan", key, value, size);
	}
	return rc;
#else
	ARG_UNUSED(key);
	ARG_UNUSED(value);
	ARG_UNUSED(size);
	return -RZI_ERR_NOT_FOUND;
#endif
}

static int write_setting(const char *key, const void *value, size_t size)
{
#if defined(CONFIG_RZI_STORAGE)
	return rzi_storage_write("lorawan", key, value, size);
#else
	ARG_UNUSED(key);
	ARG_UNUSED(value);
	ARG_UNUSED(size);
	return 0;
#endif
}

int rzi_lorawan_config_get_region(enum rzi_lorawan_region *region)
{
	uint8_t stored;
	int rc = read_setting("region", &stored, sizeof(stored));

	if (rc == -RZI_ERR_NOT_FOUND) {
		/* Old AT+BAND used RUI3 numbers, not the public region enum. */
		static const enum rzi_lorawan_region bands[] = {
			RZI_LORAWAN_REGION_EU_868,      RZI_LORAWAN_REGION_CN_470,
			RZI_LORAWAN_REGION_RU_864,      RZI_LORAWAN_REGION_IN_865,
			RZI_LORAWAN_REGION_EU_868,      RZI_LORAWAN_REGION_US_915,
			RZI_LORAWAN_REGION_AU_915,      RZI_LORAWAN_REGION_KR_920,
			RZI_LORAWAN_REGION_AS_923_GRP1, RZI_LORAWAN_REGION_AS_923_GRP2,
			RZI_LORAWAN_REGION_AS_923_GRP3, RZI_LORAWAN_REGION_AS_923_GRP4,
		};
#if defined(CONFIG_RZI_STORAGE)
		rc = rzi_storage_read("rzi", "band", &stored, sizeof(stored));
#endif
		if (rc == -RZI_ERR_NOT_FOUND) {
			return 0;
		}
		if (rc != 0) {
			return rc;
		}
		if (stored == 0U || stored >= ARRAY_SIZE(bands)) {
			return -RZI_ERR_INVALID;
		}
		stored = (uint8_t)bands[stored];
		rc = write_setting("region", &stored, sizeof(stored));
	}
	if (rc != 0) {
		return rc;
	}
	if (stored > RZI_LORAWAN_REGION_RU_864) {
		return -RZI_ERR_INVALID;
	}
	*region = (enum rzi_lorawan_region)stored;
	return 0;
}

int rzi_lorawan_config_set_region(enum rzi_lorawan_region region)
{
	uint8_t stored = (uint8_t)region;

	return write_setting("region", &stored, sizeof(stored));
}

int rzi_lorawan_get_confirmed(bool *enabled)
{
	uint8_t stored;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0 || enabled == NULL) {
		return rc != 0 ? rc : -RZI_ERR_INVALID;
	}
	k_mutex_lock(&settings_lock, K_FOREVER);
	rc = read_setting("cfm", &stored, sizeof(stored));
	if (rc == 0 && stored > 1U) {
		rc = -RZI_ERR_INVALID;
	}
	if (rc == 0) {
		confirmed = stored != 0U;
	}
	if (rc == -RZI_ERR_NOT_FOUND) {
		rc = 0;
	}
	if (rc == 0) {
		*enabled = confirmed;
	}
	k_mutex_unlock(&settings_lock);
	return rc;
}

int rzi_lorawan_set_confirmed(bool enabled)
{
	uint8_t stored = enabled ? 1U : 0U;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	k_mutex_lock(&settings_lock, K_FOREVER);
	rc = write_setting("cfm", &stored, sizeof(stored));
	if (rc == 0) {
		confirmed = enabled;
	}
	k_mutex_unlock(&settings_lock);
	return rc;
}

int rzi_lorawan_get_retries(uint8_t *count)
{
	uint8_t stored;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0 || count == NULL) {
		return rc != 0 ? rc : -RZI_ERR_INVALID;
	}
	k_mutex_lock(&settings_lock, K_FOREVER);
	rc = read_setting("rety", &stored, sizeof(stored));
	if (rc == 0 && stored > 7U) {
		rc = -RZI_ERR_INVALID;
	}
	if (rc == 0) {
		retries = stored;
	}
	if (rc == -RZI_ERR_NOT_FOUND) {
		rc = 0;
	}
	if (rc == 0) {
		*count = retries;
	}
	k_mutex_unlock(&settings_lock);
	return rc;
}

int rzi_lorawan_set_retries(uint8_t count)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0 || count > 7U) {
		return rc != 0 ? rc : -RZI_ERR_INVALID;
	}
	k_mutex_lock(&settings_lock, K_FOREVER);
	rc = write_setting("rety", &count, sizeof(count));
	if (rc == 0) {
		retries = count;
	}
	k_mutex_unlock(&settings_lock);
	return rc;
}

int rzi_lorawan_config_get_net_id(uint32_t *net_id)
{
	uint32_t stored;
	int rc;

	k_mutex_lock(&settings_lock, K_FOREVER);
	rc = read_setting("netid", &stored, sizeof(stored));
	if (rc == 0 && stored > 0xffffffU) {
		rc = -RZI_ERR_INVALID;
	}
	if (rc == 0) {
		configured_net_id = stored;
		net_id_set = true;
	}
	if (rc == -RZI_ERR_NOT_FOUND) {
		rc = 0;
	}
	if (rc == 0) {
		if (net_id_set) {
			*net_id = configured_net_id;
		} else {
			rc = -RZI_ERR_NO_DATA;
		}
	}
	k_mutex_unlock(&settings_lock);
	return rc;
}

int rzi_lorawan_set_net_id(uint32_t net_id)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0 || net_id > 0xffffffU) {
		return rc != 0 ? rc : -RZI_ERR_INVALID;
	}
	k_mutex_lock(&settings_lock, K_FOREVER);
	rc = write_setting("netid", &net_id, sizeof(net_id));
	if (rc == 0) {
		configured_net_id = net_id;
		net_id_set = true;
	}
	k_mutex_unlock(&settings_lock);
	return rc;
}

int rzi_lorawan_clear_settings(void)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
#if defined(CONFIG_RZI_STORAGE)
	static const char *const keys[] = {
		"deveui",  "joineui", "appkey",   "genappkey",     "devaddr",       "nwkskey",
		"appskey", "njm",     "autojoin", "join_interval", "join_attempts", "chmask",
		"region",  "band",    "cfm",      "rety",          "netid",
	};
	static const char *const namespaces[] = {"lorawan", "rzi"};

	k_mutex_lock(&settings_lock, K_FOREVER);
	for (size_t n = 0; n < ARRAY_SIZE(namespaces); ++n) {
		for (size_t i = 0; i < ARRAY_SIZE(keys); ++i) {
			int one = rzi_storage_delete(namespaces[n], keys[i]);

			if (one != 0 && one != -RZI_ERR_NOT_FOUND && rc == 0) {
				rc = one;
			}
		}
	}
	k_mutex_unlock(&settings_lock);
#endif
	return rc;
}
