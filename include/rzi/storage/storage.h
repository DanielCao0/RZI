/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI namespaced key-value storage API.
 */
#ifndef RZI_STORAGE_STORAGE_H
#define RZI_STORAGE_STORAGE_H

#include <stddef.h>
#include <zephyr/toolchain.h>

#include <rzi/err.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup rzi_storage RZI storage service
 *  @brief Serialized, namespaced storage independent of its physical backend.
 *  @since 0.2
 *  @version 0.4.0
 *  @{
 */

/**
 * @brief Initialize the selected storage backend.
 *
 * This operation is idempotent.
 *
 * @retval 0 Storage is ready.
 * @retval -RZI_ERR_WOULDBLOCK Called from an ISR.
 * @retval -RZI_ERR_IO Backend initialization failed.
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
 * @retval -RZI_ERR_INVALID An argument or path component is invalid.
 * @retval -RZI_ERR_NOT_FOUND The value does not exist.
 * @retval -RZI_ERR_TOO_LARGE The stored value does not have the requested size.
 * @retval -RZI_ERR_TOO_LARGE The composed backend key is too long.
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
 * @retval -RZI_ERR_INVALID An argument or path component is invalid.
 * @retval -RZI_ERR_TOO_LARGE The composed backend key is too long.
 * @retval -RZI_ERR_IO The backend write failed.
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
 * @retval -RZI_ERR_NOT_FOUND The value does not exist.
 * @retval -RZI_ERR_INVALID An argument or path component is invalid.
 * @retval -RZI_ERR_TOO_LARGE The composed backend key is too long.
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_storage_delete(const char *namespace_name, const char *key);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_STORAGE_STORAGE_H */
