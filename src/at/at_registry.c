/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>

#include "at_priv.h"

static const struct rzi_at_command *registered[CONFIG_RZI_AT_MAX_COMMANDS];
static const struct rzi_at_extension *extensions[CONFIG_RZI_AT_MAX_EXTENSIONS];
static size_t command_count;
static size_t extension_count;
static bool sealed;

static bool valid_name(const char *name)
{
	if (name == NULL || name[0] == '\0') {
		return false;
	}
	for (const char *p = name; *p != '\0'; ++p) {
		if (!((*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) {
			return false;
		}
	}
	return true;
}

int rzi_at_register(const struct rzi_at_command *commands, size_t count)
{
	if (sealed) {
		return -EACCES;
	}
	if ((commands == NULL && count != 0U) || command_count + count > ARRAY_SIZE(registered)) {
		return -EINVAL;
	}
	for (size_t i = 0; i < count; ++i) {
		if (!valid_name(commands[i].name) || commands[i].handler == NULL) {
			return -EINVAL;
		}
		for (size_t j = 0; j < command_count; ++j) {
			if (strcmp(commands[i].name, registered[j]->name) == 0) {
				return -EALREADY;
			}
		}
		for (size_t j = 0; j < i; ++j) {
			if (strcmp(commands[i].name, commands[j].name) == 0) {
				return -EALREADY;
			}
		}
	}
	for (size_t i = 0; i < count; ++i) {
		registered[command_count++] = &commands[i];
	}
	return 0;
}

void rzi_at_registry_seal(void)
{
	sealed = true;
}

int rzi_at_registry_add_extension(const struct rzi_at_extension *extension)
{
	if (extension == NULL || extension_count >= ARRAY_SIZE(extensions)) {
		return -ENOMEM;
	}
	extensions[extension_count++] = extension;
	return 0;
}

int rzi_at_registry_start_extensions(void)
{
	for (size_t i = 0; i < extension_count; ++i) {
		if (extensions[i]->start != NULL) {
			int rc = extensions[i]->start();

			if (rc != 0) {
				return rc;
			}
		}
	}
	return 0;
}

void rzi_at_registry_process_extensions(void)
{
	for (size_t i = 0; i < extension_count; ++i) {
		if (extensions[i]->process != NULL) {
			extensions[i]->process();
		}
	}
}

int rzi_at_registry_factory_reset(void)
{
	int result = 0;

	for (size_t i = 0; i < extension_count; ++i) {
		if (extensions[i]->factory_reset != NULL) {
			int rc = extensions[i]->factory_reset();

			if (rc != 0 && result == 0) {
				result = rc;
			}
		}
	}
	return result;
}

int rzi_at_dispatch(const char *name, enum rzi_at_operation operation, const char *argument)
{
	for (size_t i = 0; i < command_count; ++i) {
		const struct rzi_at_command *command = registered[i];

		if (strcmp(name, command->name) != 0) {
			continue;
		}
		if (operation == RZI_AT_OP_HELP) {
			return command->help == NULL
				       ? -ENOTSUP
				       : rzi_at_respond_value("AT+%s: %s", command->name,
							      command->help);
		}
		if ((command->allowed_operations & (1U << operation)) == 0U) {
			return -ENOTSUP;
		}
		struct rzi_at_request request = {
			.operation = operation,
			.argument = argument,
		};

		return command->handler(&request, command->user_data);
	}
	return -ENOENT;
}
