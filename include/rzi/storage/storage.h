/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI namespaced key-value storage API.
 */
#ifndef RZI_STORAGE_STORAGE_H
#define RZI_STORAGE_STORAGE_H

#include <stddef.h>
#include <stdint.h>
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
 * @retval -RZI_ERR_ISR Called from an ISR.
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
 * @retval -RZI_ERR_TOO_LARGE Stored size does not match, or the backend key is too long.
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

/** Capacity snapshot of the selected NVS backend. */
struct rzi_storage_info {
	/** Bytes managed by NVS, including its garbage-collection reserve. */
	size_t total_bytes;
	/** NVS free-space estimate, not a guaranteed payload allocation. */
	size_t free_bytes;
	/** Contiguous free bytes in the active NVS sector. */
	size_t contiguous_bytes;
	/** Erase-sector size used by NVS. */
	size_t sector_bytes;
};

/**
 * @brief Read NVS capacity and free-space estimates.
 * @param[out] info Snapshot, unchanged on failure.
 * @retval 0 Snapshot copied.
 * @retval -RZI_ERR_NOT_SUPPORTED Selected settings backend is not NVS.
 * @retval -RZI_ERR_INVALID info is NULL.
 * @note Thread context only. Names, metadata and alignment consume space;
 *       free_bytes is not a reservation. Concurrent writers may consume it.
 */
__must_check int rzi_storage_get_info(struct rzi_storage_info *info);

/** Logical namespace usage, excluding NVS alignment and metadata. */
struct rzi_storage_usage {
	/** Number of live keys. */
	size_t keys;
	/** Sum of stored value lengths, including versioned-record headers. */
	size_t value_bytes;
	/** Sum of relative key-name lengths, excluding namespace and terminators. */
	size_t name_bytes;
};

/**
 * @brief Count live keys and value bytes in one namespace subtree.
 * @param namespace_name Owner namespace, for example "app".
 * @param[out] usage Logical totals, unchanged on error.
 * @retval 0 Snapshot copied; an empty namespace has all-zero totals.
 * @retval -RZI_ERR_INVALID Invalid namespace or NULL usage.
 * @note Thread context only. This is accounting, not a quota or reservation.
 */
__must_check int rzi_storage_get_usage(const char *namespace_name, struct rzi_storage_usage *usage);

/**
 * @brief Replace one versioned, CRC-protected record as a single value.
 * @param namespace_name Owner namespace.
 * @param key Key relative to namespace_name.
 * @param version Application schema version, greater than zero.
 * @param value Encoded payload; avoid compiler-dependent struct padding.
 * @param size Payload bytes, 1 through CONFIG_RZI_STORAGE_RECORD_MAX_SIZE.
 * @retval 0 Record stored.
 * @retval -RZI_ERR_INVALID Invalid argument or zero version/size.
 * @retval -RZI_ERR_TOO_LARGE Payload exceeds the configured buffer limit.
 * @note Thread context only. Uses the same keys as raw read/write; do not mix
 *       formats at one key. Storage errors propagate; there is no multi-key
 *       transaction. Backend value limits include a 12-byte record header.
 */
__must_check int rzi_storage_write_record(const char *namespace_name, const char *key,
					  uint16_t version, const void *value, size_t size);

/**
 * @brief Validate and read one versioned record without changing outputs on error.
 * @param namespace_name Owner namespace.
 * @param key Key relative to namespace_name.
 * @param[out] version Stored application schema version.
 * @param[out] value Payload destination.
 * @param capacity Destination size.
 * @param[out] size Actual payload length.
 * @retval 0 Outputs copied after validation.
 * @retval -RZI_ERR_BAD_MESSAGE Header, length or checksum is invalid.
 * @retval -RZI_ERR_TOO_LARGE Stored record exceeds capacity or scratch limit.
 * @retval -RZI_ERR_NOT_FOUND Key does not exist.
 * @note Thread context only. The caller migrates supported old versions and
 *       rejects unknown versions before rewriting the record.
 */
__must_check int rzi_storage_read_record(const char *namespace_name, const char *key,
					 uint16_t *version, void *value, size_t capacity,
					 size_t *size);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_STORAGE_STORAGE_H */
