/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI namespaced key-value storage API.
 */
#ifndef RZI_STORAGE_H
#define RZI_STORAGE_H

#include <stddef.h>
#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup rzi_storage RZI storage service
 *  @brief Serialized, namespaced storage independent of its physical backend.
 *  @since 0.2
 *  @version 0.2.0
 *  @{
 */

/**
 * @brief Initialize the selected storage backend.
 *
 * This operation is idempotent.
 *
 * @retval 0 Storage is ready.
 * @retval -EIO Backend initialization failed.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_storage_init(void);

/**
 * @brief Read one value and require an exact size match.
 *
 * @param namespace_name Service-owned namespace.
 * @param key Key relative to namespace_name.
 * @param[out] value Destination buffer.
 * @param size Required stored and destination size.
 *
 * @retval 0 Value loaded.
 * @retval -EINVAL An argument or path component is invalid.
 * @retval -ENOENT The value does not exist.
 * @retval -EMSGSIZE The stored value does not have the requested size.
 * @retval -ENAMETOOLONG The composed backend key is too long.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_storage_read(const char *namespace_name, const char *key, void *value,
				  size_t size);

/**
 * @brief Atomically replace one namespaced value.
 *
 * @param namespace_name Service-owned namespace.
 * @param key Key relative to namespace_name.
 * @param value Value copied before returning.
 * @param size Number of bytes to store.
 *
 * @retval 0 Value stored.
 * @retval -EINVAL An argument or path component is invalid.
 * @retval -ENAMETOOLONG The composed backend key is too long.
 * @retval -EIO The backend write failed.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_storage_write(const char *namespace_name, const char *key, const void *value,
				   size_t size);

/**
 * @brief Delete one namespaced value.
 *
 * @param namespace_name Service-owned namespace.
 * @param key Key relative to namespace_name.
 *
 * @retval 0 Value deleted.
 * @retval -ENOENT The value does not exist.
 * @retval -EINVAL An argument or path component is invalid.
 * @retval -ENAMETOOLONG The composed backend key is too long.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_storage_delete(const char *namespace_name, const char *key);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_STORAGE_H */
