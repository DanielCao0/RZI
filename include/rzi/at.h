/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Transport-independent RUI3-compatible AT service.
 */
#ifndef RZI_AT_H
#define RZI_AT_H

#include <stddef.h>
#include <stdint.h>
#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup rzi_at RZI AT service
 * @brief Transport-independent AT command processing.
 * @since 0.2
 * @version 0.2.0
 * @{
 */

/** Parsed AT command operation. */
enum rzi_at_operation {
	/** Execute `AT+COMMAND`. */
	RZI_AT_OP_RUN,
	/** Describe `AT+COMMAND?`. */
	RZI_AT_OP_HELP,
	/** Read `AT+COMMAND=?`. */
	RZI_AT_OP_READ,
	/** Write `AT+COMMAND=value`. */
	RZI_AT_OP_WRITE,
};

/** Allow the run operation for a registered command. */
#define RZI_AT_ALLOW_RUN   (1U << RZI_AT_OP_RUN)
/** Allow the read operation for a registered command. */
#define RZI_AT_ALLOW_READ  (1U << RZI_AT_OP_READ)
/** Allow the write operation for a registered command. */
#define RZI_AT_ALLOW_WRITE (1U << RZI_AT_OP_WRITE)

/** Standard RUI3-compatible response status. */
enum rzi_at_status {
	/** Request accepted or synchronous operation completed. */
	RZI_AT_STATUS_OK,
	/** Unclassified command execution error. */
	RZI_AT_STATUS_ERROR,
	/** Invalid or unsupported command parameter. */
	RZI_AT_STATUS_PARAM_ERROR,
	/** A conflicting operation is in progress. */
	RZI_AT_STATUS_BUSY_ERROR,
	/** Uplink requested without an active network session. */
	RZI_AT_STATUS_NO_NETWORK_JOINED,
	/** Input exceeded a configured parser or command limit. */
	RZI_AT_STATUS_TEST_PARAM_OVERFLOW,
};

/** Parsed request passed to a registered command handler. */
struct rzi_at_request {
	/** Operation selected from the received command form. */
	enum rzi_at_operation operation;
	/** Parser-owned, NUL-terminated argument; empty when not applicable. */
	const char *argument;
};

/**
 * @brief Handle one parsed AT request.
 *
 * @param request Request valid only for the duration of this call.
 * @param user_data Opaque pointer from struct rzi_at_command.
 *
 * @return Zero on success or a negative errno value.
 *
 * @note Handlers execute serially in the AT thread and must not retain request
 *       or request->argument.
 */
typedef int (*rzi_at_command_handler_t)(const struct rzi_at_request *request, void *user_data);

/** Command names exclude the leading "AT+" and use uppercase ASCII. */
struct rzi_at_command {
	/** Static-lifetime uppercase command name. */
	const char *name;
	/** Static-lifetime one-line help text. */
	const char *help;
	/** Bitwise OR of RZI_AT_ALLOW_* values. */
	uint8_t allowed_operations;
	/** Handler invoked serially by the AT execution thread. */
	rzi_at_command_handler_t handler;
	/** Opaque application pointer passed to handler. */
	void *user_data;
};

/** Synchronous output binding used for replies and unsolicited events. */
struct rzi_at_io {
	/**
	 * @brief Write one complete response fragment.
	 *
	 * The implementation must consume data before returning and must not retain
	 * the pointer.
	 */
	int (*write)(const uint8_t *data, size_t size, void *user_data);
	/** Opaque I/O implementation pointer passed to write. */
	void *user_data;
};

/**
 * @brief Register application AT commands.
 *
 * The array and all referenced command names and help strings must remain
 * valid for the lifetime of the AT service.
 *
 * @param commands Command array, or NULL when count is zero.
 * @param count Number of entries in commands.
 *
 * @retval 0 Commands registered.
 * @retval -EACCES The registry is sealed because the service has started.
 * @retval -EALREADY A command name is already registered.
 * @retval -EINVAL The array, count, name, handler, or capacity is invalid.
 *
 * @pre Call before rzi_at_start().
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_at_register(const struct rzi_at_command *commands, size_t count);

/**
 * @brief Start the process-wide AT service with an I/O binding.
 *
 * The I/O table is copied before this function returns. The object referenced
 * by io->user_data must remain valid for the service
 * lifetime.
 *
 * @param io Synchronous output binding.
 *
 * @return Zero when started, otherwise a negative errno value from validation,
 *         command registration, or extension startup.
 *
 * @note Thread context only. Runtime stop and restart are not supported.
 * @since 0.2
 */
__must_check int rzi_at_start(const struct rzi_at_io *io);

/**
 * @brief Supply received bytes to the AT parser.
 *
 * The bytes are copied into the internal ring buffer before this function
 * returns.
 *
 * @param data Input bytes, or NULL when size is zero.
 * @param size Number of bytes to copy.
 *
 * @retval 0 All bytes accepted.
 * @retval -EAGAIN The AT service has not started.
 * @retval -EINVAL data is NULL while size is nonzero.
 * @retval -ENOSPC The ring buffer overflowed.
 *
 * @note This function is ISR-safe and non-blocking.
 * @since 0.2
 */
__must_check int rzi_at_receive(const uint8_t *data, size_t size);

/**
 * @brief Write a standard AT status response.
 *
 * @param status Response status.
 *
 * @return Zero when written, otherwise a negative errno value from validation
 *         or the active I/O binding.
 *
 * @note Thread context only.
 * @since 0.2
 */
__must_check int rzi_at_respond_status(enum rzi_at_status status);

/**
 * @brief Write a formatted AT value followed by OK.
 *
 * @param format printf-style format string.
 * @param ... Values referenced by format.
 *
 * @return Zero when written, otherwise a negative errno value from validation,
 *         formatting, or the active I/O binding.
 *
 * @note Thread context only.
 * @since 0.2
 */
__printf_like(1, 2) __must_check int rzi_at_respond_value(const char *format, ...);

/**
 * @brief Write a formatted unsolicited AT event.
 *
 * @param format printf-style format string.
 * @param ... Values referenced by format.
 *
 * @return Zero when written, otherwise a negative errno value from validation,
 *         formatting, or the active I/O binding.
 *
 * @note Thread context only.
 * @since 0.2
 */
__printf_like(1, 2) __must_check int rzi_at_publish_event(const char *format, ...);

/** @} */

#ifdef __cplusplus
}
#endif

#endif
