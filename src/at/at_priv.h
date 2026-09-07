/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RZI_AT_PRIV_H
#define RZI_AT_PRIV_H

#include <stddef.h>
#include <stdint.h>

#include <rzi/at.h>

struct rzi_at_extension {
	int (*start)(void);
	void (*process)(void);
	int (*factory_reset)(void);
};

int rzi_at_registry_add_extension(const struct rzi_at_extension *extension);
void rzi_at_registry_seal(void);
int rzi_at_registry_start_extensions(void);
void rzi_at_registry_process_extensions(void);
int rzi_at_registry_factory_reset(void);
int rzi_at_dispatch(const char *name, enum rzi_at_operation operation, const char *argument);
int rzi_at_parser_feed(uint8_t byte);
void rzi_at_parser_reset(void);
int rzi_at_builtin_register(void);
int rzi_at_write_raw(const char *text);

#endif /* RZI_AT_PRIV_H */
