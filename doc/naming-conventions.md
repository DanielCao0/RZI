# RZI file and function naming conventions

Status: normative

This document defines naming requirements for all code maintained in the RZI
repository. New code must follow these rules. Existing code touched by a
change should be brought into compliance when doing so does not break the
public API or obscure the purpose of the change.

The key words **MUST**, **MUST NOT**, **SHOULD**, and **SHOULD NOT** are
requirements for RZI contributions.

## General rules

- RZI follows the Zephyr coding style and repository `.clang-format`.
- Source identifiers and filenames MUST use English ASCII.
- Names MUST describe responsibility, not implementation history.
- Avoid abbreviations unless they are established domain terms such as
  `AT`, `EUI`, `FUOTA`, `NVM`, `UART`, `USB`, `USP`, and `LoRaWAN`.
- A public name MUST remain stable within an RZI major version.
- Do not add backend, board, vendor, or protocol-stack names to common public
  APIs unless that dependency is intentionally part of the API contract.

## Files and directories

### Format

- C source and header filenames MUST use lowercase `snake_case`.
- Directories MUST use lowercase `snake_case`.
- C implementation files use `.c`; C public and private headers use `.h`.
- Production source files and private headers MUST begin with their service
  name, for example `at_parser.c` or `lorawan_backend.h`. The repository name
  `rzi` MUST NOT be repeated in private filenames.
- Markdown and reStructuredText document filenames SHOULD use lowercase
  kebab-case, for example `naming-conventions.md`.
- A public service header SHOULD use the service name:
  `include/rzi/lorawan.h`, `include/rzi/fuota.h`.
- A subordinate public API belongs below its service:
  `include/rzi/at/uart.h`.
- A service implementation belongs below `src/<service>/`.
- A reserved or private feature header belongs beside its implementation under
  `src/<service>/<feature>/`; it MUST NOT enter `include/rzi/` before a
  callable public contract exists.
- Backend implementations belong below
  `src/<service>/backends/<service>_backend_<backend>.c`.
- Tests and samples SHOULD mirror the service directory they exercise.

Examples:

```text
include/rzi/lorawan.h
include/rzi/at/uart.h
src/lorawan/lorawan.c
src/lorawan/lorawan_backend.h
src/lorawan/backends/usp/lorawan_backend_usp.c
src/lorawan/services/multicast/lorawan_multicast.c
src/lorawan/services/multicast/lorawan_multicast.h
src/lora/lora.c
src/lora/lora.h
src/lora/lora_backend.h
src/at/at_core.c
src/at/at_priv.h
src/at/commands/at_command_system.c
src/at/commands/lorawan/at_command_lorawan.c
src/at/commands/lorawan/at_command_lorawan_key_id.c
tests/at/core/src/main.c
```

### Generic filenames

Generic production filenames such as `core.c`, `parser.c`, `registry.c`,
`internal.h`, and `util.c` MUST NOT be used. Prefix them with the service name.
`main.c` is reserved for application, sample, and test entry points.
Zephyr-defined integration filenames such as `CMakeLists.txt`, `Kconfig`, and
`module.yml` retain their required names.

Board-specific compatibility files MUST name the board and purpose, for
example `rak4631_legacy_regout.c`. Such files are temporary and SHOULD state
their removal condition.

## Public C API names

Every public RZI symbol MUST start with `rzi_` or `RZI_`.

Public declarations MUST also follow
[`api-documentation-guidelines.md`](./api-documentation-guidelines.md) for
Doxygen and compiler attributes.

### Functions

Public functions MUST use:

```text
rzi_<service>_<operation>()
```

Use a verb or established operation for the final component:

```c
rzi_lorawan_start()
rzi_lorawan_join()
rzi_lorawan_send()
rzi_lorawan_register_callbacks()
rzi_at_register()
rzi_at_uart_start()
```

- Use `init` to initialize a service and `start` to begin processing.
- Use `get_<noun>` when data is copied to an output parameter.
- Use `is_<state>` for boolean state queries.
- Use `set_<noun>` only when the operation changes an existing setting.
- Do not encode backend names in public functions.
- Do not create multiple spellings for the same operation, such as both
  `get_status` and `status_get`.

### Types

- Structure tags: `struct rzi_<service>_<noun>`.
- Enumeration tags: `enum rzi_<service>_<noun>`.
- Public callback or handler typedefs:
  `rzi_<service>_<purpose>_handler_t`.
- Public typedef names MUST end in `_t`; structure and enumeration tags MUST
  NOT end in `_t`.

Examples:

```c
struct rzi_lorawan_join_config;
struct rzi_lorawan_callbacks;
enum rzi_lorawan_region;
rzi_at_command_handler_t;
```

### Constants and enumerators

Public macros and enumeration values MUST use uppercase `SNAKE_CASE`:

```text
RZI_<SERVICE>_<NAME>
```

Examples:

```c
RZI_LORAWAN_MAX_PAYLOAD
RZI_LORAWAN_REGION_EU_868
RZI_AT_STATUS_BUSY_ERROR
```

Header guards MUST be derived from the public include path:

```text
include/rzi/lorawan.h  -> RZI_LORAWAN_H
include/rzi/at/uart.h  -> RZI_AT_UART_H
```

## Private implementation names

- File-local functions and variables MUST be `static`.
- Private functions use lowercase `snake_case` and SHOULD begin with a verb
  when they perform an action: `publish_error()`, `map_region()`,
  `handle_join()`.
- File-local names do not need the `rzi_` prefix when their file supplies
  unambiguous context.
- Private symbols shared across translation units MUST use the
  `rzi_<service>_` prefix.
- Backend operation implementations SHOULD use
  `<backend>_<operation>()`, for example `usp_init()` and `usp_send()`.
- Boolean functions and variables SHOULD use predicate names such as
  `is_joined`, `has_event`, or `enabled`.
- Output parameters SHOULD use `out` or a descriptive result name; input
  parameters MUST NOT be modified unless the API explicitly says so.

## AT command names

- Registered AT command names MUST exclude the leading `AT+`.
- Command names MUST use uppercase ASCII without spaces, for example
  `DEVEUI`, `APPKEY`, and `JOIN`.
- A command handler SHOULD be named `handle_<lowercase_command>()`.
- Command package registration functions MUST use
  `rzi_at_<package>_register()`.
- A command package with multiple command domains SHOULD use a package
  subdirectory, for example `commands/lorawan/`, and split files by stable
  command domain rather than by individual command.
- Command descriptor names, help text, allowed operations, and handlers MUST
  remain together in one `struct rzi_at_command` array. Do not create separate
  `_def.h` files that duplicate command metadata or documentation.
- Responses and unsolicited events MUST follow the documented RUI3 syntax;
  implementation-specific spellings MUST NOT leak into command names.

## Kconfig, Devicetree, and build names

- RZI Kconfig symbols MUST use `RZI_<SERVICE>_<OPTION>`.
- Internal CMake variables SHOULD use uppercase `RZI_<NAME>`.
- RZI-owned Devicetree properties use lowercase kebab-case and SHOULD carry
  an appropriate vendor prefix when defined by a binding.
- RZI sample aliases MUST use `rzi-<service>-<role>`, for example
  `rzi-at-uart`, and are referenced in C as `DT_ALIAS(rzi_at_uart)`.
- Sample and test names SHOULD start with `rzi_` when exported to Zephyr
  tooling.

## Naming review checklist

Before submitting a change, verify:

1. Files and directories use lowercase `snake_case`.
2. Public symbols use the `rzi_` or `RZI_` namespace.
3. Public functions follow `rzi_<service>_<operation>()`.
4. Private cross-file symbols include the service prefix.
5. Backend names remain private.
6. AT command names exclude `AT+` and use uppercase ASCII.
7. Header guards match their include paths.
8. `scripts/check-style.sh` passes.

Naming compliance is part of code review even when it cannot be completely
enforced by clang-format or checkpatch.
