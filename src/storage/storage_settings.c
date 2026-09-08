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

#include <rzi/storage.h>

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
		return -EINVAL;
	}
	length = snprintk(name, name_size, "%s/%s", namespace_name, key);
	if (length < 0 || (size_t)length >= name_size) {
		return -ENAMETOOLONG;
	}
	return 0;
}

int rzi_storage_init(void)
{
	int rc = 0;

	k_mutex_lock(&storage_lock, K_FOREVER);
	if (!initialized) {
		rc = settings_subsys_init();
		if (rc == 0) {
			initialized = true;
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
		return -EINVAL;
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
		return (int)bytes;
	}
	if (bytes == 0) {
		return -ENOENT;
	}
	return bytes == (ssize_t)size ? 0 : -EMSGSIZE;
}

int rzi_storage_write(const char *namespace_name, const char *key, const void *value, size_t size)
{
	char name[RZI_STORAGE_NAME_MAX];
	int rc;

	if (value == NULL || size == 0U) {
		return -EINVAL;
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
	rc = settings_save_one(name, value, size);
	k_mutex_unlock(&storage_lock);
	return rc;
}

int rzi_storage_delete(const char *namespace_name, const char *key)
{
	char name[RZI_STORAGE_NAME_MAX];
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
	rc = settings_delete(name);
	k_mutex_unlock(&storage_lock);
	return rc;
}
