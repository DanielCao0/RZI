/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Closed RZI public error catalog.
 */
#ifndef RZI_ERR_H
#define RZI_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup rzi_err RZI error codes
 * @brief Closed failure codes returned by public RZI APIs.
 * @since 0.4
 * @version 0.4.0
 * @{
 */

/**
 * Closed public error catalog.
 *
 * Fallible public functions return `0` on success or the negation of one of
 * these values. Catalog values start at 1000 so a negative `RZI_ERR_*` cannot
 * equal a negative POSIX errno. Zephyr's minimal libc defines `EWOULDBLOCK`
 * as `EAGAIN`, so those POSIX names cannot distinguish an ISR call from a
 * service that is not ready.
 */
enum rzi_err {
	/** Request accepted or operation completed. */
	RZI_SUCCESS = 0,
	/** Invalid argument, enum, port, or payload field. */
	RZI_ERR_INVALID = 1000,
	/** A thread-context API was called from an ISR. */
	RZI_ERR_ISR = 1001,
	/** The service or backend has not started or is not ready. */
	RZI_ERR_NOT_READY = 1002,
	/** Repeated start, or the object is already registered. */
	RZI_ERR_ALREADY = 1003,
	/** A conflicting asynchronous operation is already in flight. */
	RZI_ERR_BUSY = 1004,
	/** The selected backend does not support the requested capability. */
	RZI_ERR_NOT_SUPPORTED = 1005,
	/** A fixed subscriber slot, buffer, or table is full. */
	RZI_ERR_NO_RESOURCE = 1006,
	/** Handle, key, session, or image does not exist. */
	RZI_ERR_NOT_FOUND = 1007,
	/** No downlink, beacon, or reconstructed image size is available. */
	RZI_ERR_NO_DATA = 1008,
	/** Payload, key, or stored value exceeds the accepted size. */
	RZI_ERR_TOO_LARGE = 1009,
	/** The operation timed out. */
	RZI_ERR_TIMEOUT = 1010,
	/** An internal event or input queue overflowed. */
	RZI_ERR_OVERFLOW = 1011,
	/** Backend or hardware failure without a more precise code. */
	RZI_ERR_IO = 1012,
	/** CRC or protocol contents are damaged. */
	RZI_ERR_BAD_MESSAGE = 1013,
	/** Required device is missing. */
	RZI_ERR_NO_DEVICE = 1014,
	/** The request requires an active network session. */
	RZI_ERR_NOT_JOINED = 1015,
	/** The caller is not allowed to perform this operation. */
	RZI_ERR_DENIED = 1016,
	/** The service lifecycle does not allow this operation. */
	RZI_ERR_INVALID_STATE = 1017,
};

/**
 * @brief Return a static name for a public error code.
 *
 * Accepts `0`, a positive @ref rzi_err value, or the negative value returned
 * by a public API.
 *
 * @param err Success or error value.
 *
 * @return Static NUL-terminated name, or `"RZI_ERR_UNKNOWN"`.
 * @since 0.4
 */
const char *rzi_err_str(int err);

/**
 * @brief Map a vendor or POSIX errno to the closed RZI catalog.
 *
 * Zero and positive values become `0`. A value that is already a negative
 * catalog code is returned unchanged. Codes that are not in the catalog
 * become `-RZI_ERR_IO`. `EAGAIN` and `EWOULDBLOCK` both map to
 * `-RZI_ERR_NOT_READY` because those POSIX values are aliases on Zephyr.
 * `ENOSYS` maps to `-RZI_ERR_NOT_SUPPORTED`, `EEXIST` to `-RZI_ERR_ALREADY`,
 * `EPERM`/`EACCES` to `-RZI_ERR_DENIED`, `ENOSPC` to `-RZI_ERR_NO_RESOURCE`,
 * and `ENETDOWN` to `-RZI_ERR_NOT_READY`. This helper does not produce
 * `-RZI_ERR_NOT_JOINED` or `-RZI_ERR_ISR`.
 *
 * Public service entry points must not call this for their own argument
 * checks. Use it only at a backend or driver boundary.
 *
 * @param rc Vendor or POSIX return value, or an already mapped RZI error.
 *
 * @return `0` or a negative @ref rzi_err value.
 * @since 0.4
 */
int rzi_err_from_errno(int rc);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* RZI_ERR_H */
