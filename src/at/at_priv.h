/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Internal contracts shared by RZI AT service components.
 */
#ifndef RZI_AT_PRIV_H
#define RZI_AT_PRIV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <rzi/at/at.h>

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
 * @return Zero on success, otherwise a negative RZI_ERR_* value.
 */
void rzi_at_registry_seal(void);
int rzi_at_registry_add_extension(const struct rzi_at_extension *extension);
int rzi_at_registry_start_extensions(void);
void rzi_at_registry_process_extensions(void);
int rzi_at_registry_factory_reset(void);
int rzi_at_dispatch(const char *name, enum rzi_at_operation operation, const char *argument);
int rzi_at_parser_feed(uint8_t byte);
void rzi_at_parser_reset(void);
int rzi_at_builtin_register(void);
int rzi_at_write_raw(const char *text);

/** Write help for every registered command, then OK. */
int rzi_at_registry_write_all_help(void);

/** True when AT+LOCK is active. */
bool rzi_at_is_locked(void);

/** Lock or unlock the AT port. */
void rzi_at_set_locked(bool locked);

/** True when input echo is enabled. */
bool rzi_at_echo_enabled(void);

/** Enable or disable input echo. */
void rzi_at_set_echo(bool enabled);

/** Compare the serial-port password. */
bool rzi_at_password_matches(const char *password);

/** Store the serial-port password, max 8 characters. */
int rzi_at_set_password(const char *password);

/** Current serial-port password, never NULL. */
const char *rzi_at_password(void);

#endif /* RZI_AT_PRIV_H */
