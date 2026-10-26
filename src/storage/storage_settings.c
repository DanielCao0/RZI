/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Zephyr settings backend for the RZI storage service.
 */

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#if defined(CONFIG_SETTINGS_NVS)
#include <zephyr/kvss/nvs.h>
#endif

#include <rzi/err.h>
#include <rzi/storage/storage.h>

#if defined(CONFIG_RZI_POWER) && defined(CONFIG_RZI_POWER_AUTO_SERVICE_LOCK)
#include <rzi/power/power.h>

static __maybe_unused void ignore_result(int result)
{
	ARG_UNUSED(result);
}
#endif

#define RZI_STORAGE_NAME_MAX 64

K_MUTEX_DEFINE(storage_lock);

static bool initialized;

static bool valid_component(const char *component)
{
	size_t length;

	if (component == NULL || component[0] == '\0' || component[0] == '/') {
		return false;
	}
	length = strlen(component);
	return component[length - 1U] != '/' && strstr(component, "//") == NULL;
}

static int build_name(char *name, size_t name_size, const char *namespace_name, const char *key)
{
	int length;

	if (!valid_component(namespace_name) || !valid_component(key)) {
		return -RZI_ERR_INVALID;
	}
	length = snprintk(name, name_size, "%s/%s", namespace_name, key);
	if (length < 0 || (size_t)length >= name_size) {
		return -RZI_ERR_TOO_LARGE;
	}
	return 0;
}

int rzi_storage_init(void)
{
	int rc = 0;

	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}

	k_mutex_lock(&storage_lock, K_FOREVER);
	if (!initialized) {
		rc = settings_subsys_init();
		if (rc == 0) {
			initialized = true;
		} else {
			rc = rzi_err_from_errno(rc);
		}
	}
	k_mutex_unlock(&storage_lock);
	return rc;
}

int rzi_storage_read(const char *namespace_name, const char *key, void *value, size_t size)
{
	char name[RZI_STORAGE_NAME_MAX];
	ssize_t bytes;
	int rc;

	if (value == NULL || size == 0U) {
		return -RZI_ERR_INVALID;
	}
	rc = build_name(name, sizeof(name), namespace_name, key);
	if (rc != 0) {
		return rc;
	}
	rc = rzi_storage_init();
	if (rc != 0) {
		return rc;
	}
	k_mutex_lock(&storage_lock, K_FOREVER);
	bytes = settings_load_one(name, value, size);
	k_mutex_unlock(&storage_lock);
	if (bytes < 0) {
		return rzi_err_from_errno((int)bytes);
	}
	if (bytes == 0) {
		return -RZI_ERR_NOT_FOUND;
	}
	return bytes == (ssize_t)size ? 0 : -RZI_ERR_TOO_LARGE;
}

int rzi_storage_write(const char *namespace_name, const char *key, const void *value, size_t size)
{
	char name[RZI_STORAGE_NAME_MAX];
	int rc;

	if (value == NULL || size == 0U) {
		return -RZI_ERR_INVALID;
	}
	rc = build_name(name, sizeof(name), namespace_name, key);
	if (rc != 0) {
		return rc;
	}
	rc = rzi_storage_init();
	if (rc != 0) {
		return rc;
	}
	k_mutex_lock(&storage_lock, K_FOREVER);
#if defined(CONFIG_RZI_POWER) && defined(CONFIG_RZI_POWER_AUTO_SERVICE_LOCK)
	rc = rzi_power_lock_get(RZI_POWER_LOCK_FLASH);
	if (rc != 0) {
		k_mutex_unlock(&storage_lock);
		return rc;
	}
#endif
	rc = settings_save_one(name, value, size);
#if defined(CONFIG_RZI_POWER) && defined(CONFIG_RZI_POWER_AUTO_SERVICE_LOCK)
	ignore_result(rzi_power_lock_put(RZI_POWER_LOCK_FLASH));
#endif
	k_mutex_unlock(&storage_lock);
	return rzi_err_from_errno(rc);
}

int rzi_storage_delete(const char *namespace_name, const char *key)
{
	char name[RZI_STORAGE_NAME_MAX];
	ssize_t stored_size;
	int rc;

	rc = build_name(name, sizeof(name), namespace_name, key);
	if (rc != 0) {
		return rc;
	}
	rc = rzi_storage_init();
	if (rc != 0) {
		return rc;
	}
	k_mutex_lock(&storage_lock, K_FOREVER);
#if defined(CONFIG_RZI_POWER) && defined(CONFIG_RZI_POWER_AUTO_SERVICE_LOCK)
	rc = rzi_power_lock_get(RZI_POWER_LOCK_FLASH);
	if (rc != 0) {
		k_mutex_unlock(&storage_lock);
		return rc;
	}
#endif
	stored_size = settings_get_val_len(name);
	if (stored_size < 0) {
		rc = (int)stored_size;
	} else if (stored_size == 0) {
		rc = -ENOENT;
	} else {
		rc = settings_delete(name);
	}
#if defined(CONFIG_RZI_POWER) && defined(CONFIG_RZI_POWER_AUTO_SERVICE_LOCK)
	ignore_result(rzi_power_lock_put(RZI_POWER_LOCK_FLASH));
#endif
	k_mutex_unlock(&storage_lock);
	return rzi_err_from_errno(rc);
}

#define RECORD_HEADER_SIZE 12U
#define RECORD_MAGIC       0x31525a52U

static uint32_t record_crc(const uint8_t *record, size_t size)
{
	uint32_t crc = crc32_ieee(record, 8U);

	return crc32_ieee_update(crc, record + RECORD_HEADER_SIZE, size);
}

int rzi_storage_write_record(const char *namespace_name, const char *key, uint16_t version,
			     const void *value, size_t size)
{
	uint8_t record[RECORD_HEADER_SIZE + CONFIG_RZI_STORAGE_RECORD_MAX_SIZE];

	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}
	if (version == 0U || value == NULL || size == 0U) {
		return -RZI_ERR_INVALID;
	}
	if (size > CONFIG_RZI_STORAGE_RECORD_MAX_SIZE) {
		return -RZI_ERR_TOO_LARGE;
	}
	sys_put_le32(RECORD_MAGIC, record);
	sys_put_le16(version, record + 4U);
	sys_put_le16((uint16_t)size, record + 6U);
	memcpy(record + RECORD_HEADER_SIZE, value, size);
	sys_put_le32(record_crc(record, size), record + 8U);
	return rzi_storage_write(namespace_name, key, record, RECORD_HEADER_SIZE + size);
}

int rzi_storage_read_record(const char *namespace_name, const char *key, uint16_t *version,
			    void *value, size_t capacity, size_t *size)
{
	uint8_t record[RECORD_HEADER_SIZE + CONFIG_RZI_STORAGE_RECORD_MAX_SIZE];
	char name[RZI_STORAGE_NAME_MAX];
	ssize_t length;
	size_t payload_size;
	int rc;

	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}
	if (version == NULL || value == NULL || size == NULL || capacity == 0U) {
		return -RZI_ERR_INVALID;
	}
	rc = build_name(name, sizeof(name), namespace_name, key);
	if (rc != 0) {
		return rc;
	}
	rc = rzi_storage_init();
	if (rc != 0) {
		return rc;
	}
	/* Size discovery and read share the lock, including concurrent raw writers. */
	k_mutex_lock(&storage_lock, K_FOREVER);
	length = settings_get_val_len(name);
	if (length > (ssize_t)sizeof(record)) {
		rc = -RZI_ERR_TOO_LARGE;
	} else if (length < 0) {
		rc = rzi_err_from_errno((int)length);
	} else if (length == 0) {
		rc = -RZI_ERR_NOT_FOUND;
	} else {
		ssize_t actual = settings_load_one(name, record, (size_t)length);

		rc = actual < 0         ? rzi_err_from_errno((int)actual)
		     : actual != length ? -RZI_ERR_IO
					: 0;
	}
	k_mutex_unlock(&storage_lock);
	if (rc != 0) {
		return rc;
	}
	if (length <= RECORD_HEADER_SIZE || sys_get_le32(record) != RECORD_MAGIC ||
	    sys_get_le16(record + 4U) == 0U) {
		return -RZI_ERR_BAD_MESSAGE;
	}
	payload_size = sys_get_le16(record + 6U);
	if (payload_size != (size_t)length - RECORD_HEADER_SIZE ||
	    sys_get_le32(record + 8U) != record_crc(record, payload_size)) {
		return -RZI_ERR_BAD_MESSAGE;
	}
	if (payload_size > capacity) {
		return -RZI_ERR_TOO_LARGE;
	}
	memcpy(value, record + RECORD_HEADER_SIZE, payload_size);
	*size = payload_size;
	*version = sys_get_le16(record + 4U);
	return 0;
}

int rzi_storage_get_info(struct rzi_storage_info *info)
{
	int rc;

	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}
	if (info == NULL) {
		return -RZI_ERR_INVALID;
	}
#if defined(CONFIG_SETTINGS_NVS)
	struct rzi_storage_info snapshot;
	struct nvs_fs *fs;
	void *storage = NULL;
	ssize_t available;

	rc = rzi_storage_init();
	if (rc != 0) {
		return rc;
	}
	k_mutex_lock(&storage_lock, K_FOREVER);
	rc = settings_storage_get(&storage);
	if (rc == 0 && storage == NULL) {
		rc = -ENOTSUP;
	}
	if (rc == 0) {
		fs = storage;
		available = nvs_calc_free_space(fs);
		if (available < 0) {
			rc = (int)available;
		} else {
			snapshot.total_bytes = (size_t)fs->sector_size * fs->sector_count;
			snapshot.free_bytes = (size_t)available;
			snapshot.contiguous_bytes = nvs_sector_max_data_size(fs);
			snapshot.sector_bytes = fs->sector_size;
			*info = snapshot;
		}
	}
	k_mutex_unlock(&storage_lock);
	return rzi_err_from_errno(rc);
#else
	ARG_UNUSED(rc);
	return -RZI_ERR_NOT_SUPPORTED;
#endif
}

static int count_usage(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg,
		       void *param)
{
	struct rzi_storage_usage *usage = param;

	ARG_UNUSED(read_cb);
	ARG_UNUSED(cb_arg);
	if (len != 0U) {
		usage->keys++;
		usage->value_bytes += len;
		usage->name_bytes += key != NULL ? strlen(key) : 0U;
	}
	return 0;
}

int rzi_storage_get_usage(const char *namespace_name, struct rzi_storage_usage *usage)
{
	struct rzi_storage_usage snapshot = {0};
	int rc;

	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}
	if (usage == NULL || !valid_component(namespace_name)) {
		return -RZI_ERR_INVALID;
	}
	rc = rzi_storage_init();
	if (rc != 0) {
		return rc;
	}
	k_mutex_lock(&storage_lock, K_FOREVER);
	rc = settings_load_subtree_direct(namespace_name, count_usage, &snapshot);
	k_mutex_unlock(&storage_lock);
	if (rc == 0) {
		*usage = snapshot;
	}
	return rzi_err_from_errno(rc);
}
