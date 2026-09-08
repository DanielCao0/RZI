/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Internal contracts shared by RZI AT service components.
 */
#ifndef RZI_AT_PRIV_H
#define RZI_AT_PRIV_H

#include <stddef.h>
#include <stdint.h>

#include <rzi/at.h>

/**
 * @brief Lifecycle hooks implemented by an internal AT extension.
 *
 * All hooks execute in thread context. A NULL hook is ignored.
 */
struct rzi_at_extension {
	/** Initialize the extension before the AT execution thread starts. */
	int (*start)(void);
	/** Perform optional periodic processing in the AT execution thread. */
	void (*process)(void);
	/** Delete persistent extension state before a factory-reset reboot. */
	int (*factory_reset)(void);
};

/**
 * @brief Add an internal extension before the registry is sealed.
 *
 * @param extension Static-lifetime extension descriptor.
 * @return Zero on success, otherwise a negative errno value.
 */
int rzi_at_registry_add_extension(const struct rzi_at_extension *extension);

/** @brief Prevent subsequent command and extension registration. */
void rzi_at_registry_seal(void);

/**
 * @brief Start all registered extensions.
 *
 * @return Zero on success, otherwise the first extension error.
 */
int rzi_at_registry_start_extensions(void);

/** @brief Run periodic hooks for all registered extensions. */
void rzi_at_registry_process_extensions(void);

/**
 * @brief Request factory reset from all registered extensions.
 *
 * @return Zero on success, otherwise the first extension error.
 */
int rzi_at_registry_factory_reset(void);

/**
 * @brief Dispatch a parsed operation to a registered command.
 *
 * @param name Uppercase command name without the `AT+` prefix.
 * @param operation Parsed operation.
 * @param argument Mutable, parser-owned argument valid for the handler call.
 * @return Zero on success, otherwise a negative errno value.
 */
int rzi_at_dispatch(const char *name, enum rzi_at_operation operation, const char *argument);

/**
 * @brief Feed one byte into the line parser.
 *
 * @param byte Received byte.
 * @return Zero on success, otherwise a negative errno value.
 */
int rzi_at_parser_feed(uint8_t byte);

/** @brief Discard the current partial parser input. */
void rzi_at_parser_reset(void);

/**
 * @brief Register built-in system and configured service commands.
 *
 * @return Zero on success, otherwise a negative errno value.
 */
int rzi_at_builtin_register(void);

/**
 * @brief Write preformatted text through the active serialized I/O binding.
 *
 * @param text NUL-terminated text to write synchronously.
 * @return Zero on success, otherwise a negative errno value.
 */
int rzi_at_write_raw(const char *text);

#endif /* RZI_AT_PRIV_H */
