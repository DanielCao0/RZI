# Coding standards

Status: normative

New code in this repository must follow this document. When touching older
code, bring it into compliance in the same change when that does not break
the public API or mix unrelated edits into one commit.

See also: [api-docs.md](./api-docs.md), [architecture.md](./architecture.md).

The keywords **must**, **must not**, **should**, and **should not** have the
RFC 2119 meanings of MUST / MUST NOT / SHOULD / SHOULD NOT.

Naming, directories, public symbols, AT, and Kconfig follow this document.
Doxygen and compiler-annotation details for public APIs are in
[api-docs.md](./api-docs.md). Architecture and ownership are in
[architecture.md](./architecture.md).

## 1. Relationship to Zephyr

RZI is a Zephyr module and does not invent a second formatting style:

- Indentation, wrapping, and braces follow the repository-root
  `.clang-format` (a Zephyr copy).
- `scripts/check-style.sh` runs clang-format, production-library
  `@file` / `@brief` checks, and Zephyr `checkpatch.pl` (100-column width).
- Public headers use Zephyr-style Doxygen. Do not invent a private tag set.
- USP, LBM, loramac-node, or board symbols must not leak into the public ABI.

Local checks:

```bash
scripts/check-style.sh
scripts/check-style.sh --fix
scripts/generate-doxygen.sh
scripts/generate-sbom.sh
```

## 2. Repository layering

| Location | What belongs there |
|---|---|
| `include/rzi/` | Public C ABI. Applications may only `#include <rzi/...>` |
| `src/<service>/` | Implementation and private headers for that service |
| `src/system/` | Module-wide errors, version, and compiled service capabilities. Not a product service |
| `samples/`, `tests/` | Samples and tests; mirror the service under test |
| `doc/` | Standards and architecture. Lowercase kebab-case names; do not repeat `rzi` |

Public headers use one subdirectory per service, matching Zephyr
`include/zephyr/<subsystem>/` and USP `include/zephyr/usp/`:

```text
include/rzi/
├── version.h
├── capabilities.h
├── lorawan/lorawan.h
├── lorawan/fuota.h
├── lorawan/multicast.h
├── lorawan/certification.h
├── lora/lora.h
├── power/power.h
├── storage/storage.h
├── at/at.h
└── at/uart.h
```

LoRaWAN implementation is split by responsibility:

```text
src/lorawan/
├── lorawan*.c      Public API, event dispatch, LinkCheck / DeviceTime / thin facades
├── backend/        Private contract plus USP / Zephyr implementations
└── services/fuota/ Optional FUOTA coordinator
```

Private headers must sit next to the implementation. They must not enter
`include/rzi/` before a callable public contract exists.

## 3. Naming

### 3.1 General rules

- Source identifiers and file names must be English ASCII.
- Names describe responsibility, not implementation history. Do not keep an
  old protocol name or a one-off migration codename in a new file name.
- Do not invent abbreviations. Established domain words may stay: `AT`,
  `EUI`, `FUOTA`, `NVM`, `UART`, `USB`, `USP`, `LoRaWAN`.
- Public names must stay stable within one RZI major version.
- Unless that dependency is part of the API contract, public APIs must not
  carry a backend, board, vendor, or protocol-stack name.

### 3.2 Files and directories

- C sources, headers, and directories: lowercase `snake_case`.
- Implementations use `.c`. Public and private headers use `.h`.
- A production-library basename must remain recognizable without its path.
  Start it with the nearest stable service or package name, for example
  `at_parser.c`, `at_lorawan_join_send.c`, and `lorawan_backend.h`. Do not
  mechanically repeat structural directories such as `commands` in every
  nested file. Private file names must not repeat the repository name `rzi`.
- Backend operation-table headers end in `_ops.h`, matching their
  `struct ..._ops` type.
- Documentation: lowercase kebab-case, for example `coding-standards.md`.
- Public service headers: `include/rzi/<service>/<name>.h`.
- Dependent public APIs hang under their owning service:
  `include/rzi/at/uart.h`.
- Module-level headers may stay at `include/rzi/`: `version.h`,
  `capabilities.h`.
- Backend implementations:
  `src/<service>/backend/<service>_backend_<backend>.c`.
- Tests and samples start with the service name and then describe the behavior,
  component, or scenario under test. Do not use generic `core` directories.
  Use `api` only for a test that exclusively verifies a public API contract.

```text
include/rzi/lorawan/lorawan.h
include/rzi/lora/lora.h
include/rzi/at/uart.h
src/lorawan/lorawan.c
src/lorawan/backend/lorawan_backend.h
src/lorawan/backend/usp/lorawan_backend_usp.c
src/lora/lora.c
src/lora/backend/lora_backend.h
src/at/at.c
src/at/at_priv.h
src/at/commands/lorawan/at_lorawan.c
tests/lorawan/service/src/main.c
```

Production-library files must not use unprefixed generic names: `core.c`,
`parser.c`, `registry.c`, `internal.h`, `util.c`. Add the service name.
Reserve `main.c` for application, sample, and test entry points. Keep Zephyr
prescribed names: `CMakeLists.txt`, `Kconfig`, `module.yml`.

Board compatibility files must name the board and the purpose, for example
`rak4631_legacy_regout.c`, and must document the deletion condition.

### 3.3 Public C API

Every public RZI symbol must start with `rzi_` or `RZI_`.

Functions:

```text
rzi_<service>_<operation>()
```

```c
rzi_lorawan_start()
rzi_lorawan_join()
rzi_lorawan_send()
rzi_lorawan_register_callbacks()
rzi_power_set_policy()
rzi_at_uart_start()
```

- Use `init` for initialization and `start` to begin processing.
- Use `get_<noun>` when copying into an output parameter.
- Use `is_<state>` for boolean queries.
- Use `set_<noun>` only when changing an existing setting.
- Do not keep two spellings of the same operation (no `get_status` and
  `status_get` together).
- Public functions must not carry a backend name.

Types:

| Kind | Form |
|---|---|
| Struct tag | `struct rzi_<service>_<noun>` |
| Enum tag | `enum rzi_<service>_<noun>` |
| Callback / handler typedef | `rzi_<service>_<purpose>_handler_t` |

Typedefs must end in `_t`. Struct and enum tags must not end in `_t`.

Constants and enumerators: uppercase `RZI_<SERVICE>_<NAME>`.

```c
RZI_LORAWAN_MAX_PAYLOAD
RZI_LORAWAN_REGION_EU_868
RZI_POWER_POLICY_SUSPEND
```

Header guards must be derived from the public include path:

```text
include/rzi/lorawan/lorawan.h  -> RZI_LORAWAN_LORAWAN_H
include/rzi/at/uart.h          -> RZI_AT_UART_H
include/rzi/power/power.h      -> RZI_POWER_POWER_H
```

### 3.4 Private implementation

- File-local functions and variables must be `static`.
- Private functions use lowercase `snake_case`. Actions should start with a
  verb: `publish_error()`, `map_region()`, `handle_join()`.
- File-local names need not add `rzi_` when the file context is enough.
- Private symbols that cross translation units must use the
  `rzi_<service>_` prefix.
- Backend operation implementations should use `<backend>_<operation>()`,
  for example `usp_init()` and `usp_send()`.
- Booleans use predicates: `is_joined`, `has_event`, `enabled`.
- Output parameters use `out` or an explicit result name. Do not mutate
  input objects unless the API says so.

### 3.5 AT commands

- Registered names must not include a leading `AT+`.
- Command names must be uppercase ASCII with no spaces: `DEVEUI`, `APPKEY`,
  `JOIN`.
- Handlers should be named `handle_<lowercase_command>()`.
- Command-package registration functions must be
  `rzi_at_<package>_register()`.
- Multi-domain packages should use subdirectories (`commands/lorawan/`) and
  split files by stable domain. Do not create one file per command.
- Descriptors, help, allowed operations, and handlers must stay in one
  `struct rzi_at_command` array. Do not invent a `_def.h` copy of the
  metadata.
- Responses and unsolicited events must follow the documented RUI3 syntax.
  Implementation-private names must not leak into command names.

### 3.6 Kconfig, Devicetree, and build

- Kconfig: `RZI_<SERVICE>_<OPTION>`. One `zephyr/Kconfig.<service>` file
  per service (`storage`, `lorawan`, `lora`, `at`, `power`). The
  MCUboot contract lives in `zephyr/Kconfig.boot`. Child options keep
  the full parent prefix, for example `RZI_LORAWAN_FUOTA_*` under
  `RZI_LORAWAN_FUOTA`, and `RZI_LORAWAN_BACKEND_ZEPHYR_*` under
  `RZI_LORAWAN_BACKEND_ZEPHYR`.
- Internal CMake variables should be uppercase `RZI_<NAME>`.
- RZI-owned DTS properties use lowercase kebab-case. Bindings should carry
  an appropriate vendor prefix.
- Sample aliases: `rzi-<service>-<role>`; in C that is
  `DT_ALIAS(rzi_at_uart)`.
- Sample and test names handed to Zephyr tools should start with `rzi_`.

## 4. Headers and includes

Applications and samples include only public headers:

```c
#include <rzi/lorawan/lorawan.h>
#include <rzi/power/power.h>
#include <rzi/at/at.h>
```

- Public headers must have `#ifndef` guards and C++ `extern "C"` guards.
- Public headers may contain RZI types, standard C types, and only the
  unavoidable stable Zephyr types at I/O boundaries (for example
  `struct device`).
- Implementations include each other with relative paths or the current
  service's private headers. Do not put private contracts in `include/rzi/`.
- Include order: matching module header, C standard / POSIX, Zephyr, RZI
  public headers, this service's private headers. Keep blank-line grouping
  consistent with the existing file; clang-format finishes the rest.

## 5. Errors, return values, and asynchrony

Public functions:

- Return `0` on success and a negative `enum rzi_err` on failure.
  See [error-codes.md](./error-codes.md). Do not return POSIX errno.
- Validate arguments before entering a backend.
- If `0` only means "request accepted", the header and `@retval` must say
  so. Completion uses a typed event callback; applications must not guess.
- Asynchronous request data must be copied before return, or the lifetime
  must be fixed in the contract.
- Avoid heap allocation in the base configuration.
- Remain source-compatible within one major version.

USP / LBM / loramac-node types, enums, or thread models must not appear in
public headers or public types.

## 6. Concurrency and context

- Default: public functions are thread-context only. ISR callers return
  `-RZI_ERR_WOULDBLOCK`.
- Callbacks run serially on the documented thread (LoRaWAN uses the
  dispatcher thread). Do not assume they may block or re-enter arbitrarily.
- Pointers passed to a callback are valid only for that invocation. Data
  that must outlive the call must be copied.
- Cross-thread queues have a fixed length. Overflow must be observable.
  Silent loss without a query path is not allowed.
- Lock scope belongs in implementation comments. Do not call a public API
  that can re-enter the same service while holding a lock, unless that API
  explicitly allows it.

## 7. Comments and Doxygen

Every production-library `.c` / `.h` must have:

```c
/**
 * @file
 * @brief One-line file purpose.
 */
```

Public headers also need `@defgroup`, `@since`, `@version`, and a closing
`/** @} */` at the end of the file. Private `.c` files must not define a
public group.

Public functions must document `@brief`, `@param`, `@retval` / `@return`,
thread context, and asynchronous semantics. Details and `__must_check` /
`__printf_like` / `__deprecated` are in [api-docs.md](./api-docs.md).

Private `static` functions do not need mechanical Doxygen. Explain *why*,
not a restatement of the name.

Comments and public documentation strings are English, matching the existing
headers and Zephyr. Markdown under `doc/` is English.

## 8. Tests and samples

- Test directories name the subject or scenario: `tests/lorawan/service`,
  `tests/at/framework`, and `tests/power/api`.
- When testing a private contract, add `src/<service>` to the include path
  and include private headers such as `backend/lorawan_backend.h`. Do not
  copy the contract for the test.
- Samples demonstrate the public API. They do not bypass the service to call
  a backend.
- `samples/` and `tests/` do not require file-level Doxygen.
- Low-power samples must not `sleep(fixed duration)` during an in-flight
  Class A exchange to race the RX windows. Join and send from callbacks,
  schedule later uplinks with delayed work, and leave the main thread blocked.

## 9. Commits

Commit messages follow Conventional Commits:

```text
<type>(<scope>): <subject>
```

Common types: `feat`, `fix`, `docs`, `refactor`, `test`, `build`. Scope is
the service name (`lorawan`, `power`, `at`). The subject is
imperative, lowercase, and has no trailing period.

One commit does one thing. Format with `scripts/check-style.sh --fix`. Do
not mix clang-format and logic changes in the same "while we are here"
commit unless the surface area is tiny.

## 10. Review checklist

- [ ] Files and directories are lowercase `snake_case`. Production-library
      files carry a service prefix.
- [ ] Public headers live under `include/rzi/<service>/`. Applications only
      `#include <rzi/...>`.
- [ ] Public symbols are `rzi_` / `RZI_`. Functions are
      `rzi_<service>_<operation>()`.
- [ ] Header guards match the include path.
- [ ] Backend / board / protocol-stack names did not enter the public API.
- [ ] AT command names have no `AT+` and are uppercase ASCII.
- [ ] Return values and asynchronous completion are not mixed. Errors are
      negative `RZI_ERR_*` values.
- [ ] Thread / ISR / callback context is documented.
- [ ] Production-library `.c` / `.h` files have `@file` and `@brief`.
- [ ] `scripts/check-style.sh` passes.

Naming and layering are code-review items even when tools cannot detect
them.
