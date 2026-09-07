# RZI

RZI is an independent Zephyr module that exposes RAK-oriented C APIs while
keeping protocol-stack integration private and relying on upstream Zephyr
board support. The first implemented service is LoRaWAN over Semtech USP and
LoRa Basics Modem.

The target architecture, ownership boundaries, backend migration policy, and
service roadmap are defined in
[`doc/rzi-sdk-architecture.md`](doc/rzi-sdk-architecture.md).

## Repository layout

```text
rzi/
├── zephyr/
│   ├── module.yml       Zephyr module metadata
│   ├── Kconfig          RZI feature configuration
│   ├── CMakeLists.txt   Zephyr library integration
│   ├── patches.yml      Backend compatibility patch manifest
│   └── patches/         Backend compatibility patches
├── include/rzi/         Public C API
├── src/
│   ├── lorawan/
│   │   ├── lorawan.c          Backend-independent service
│   │   ├── lorawan_backend.h  Private backend contract
│   │   └── backends/
│   │       └── lorawan_backend_usp.c
│   │                              Semtech USP implementation
│   └── at/
│       ├── at_core.c          Lifecycle, RX queue and output
│       ├── at_parser.c        RUI3 command grammar
│       ├── at_registry.c      Extensible command registry
│       ├── commands/          Optional RZI service command packages
│       └── transports/        Optional UART and future transports
├── samples/             RZI-owned buildable samples
├── doc/                 Architecture and internal documentation
├── LICENSE
└── README.md
```

Zephyr discovers RZI through [`zephyr/module.yml`](zephyr/module.yml). There is
no SDK wrapper directory and applications do not add RZI sources manually.

## Customer integration

A customer application adds RZI as a project in its own `west.yml` and
imports the RZI manifest:

```yaml
manifest:
  projects:
    - name: rzi
      url: https://github.com/DanielCao0/RZI
      revision: main
      path: rzi
      import: true
```

The import pulls the default USP backend dependencies (`usp_zephyr` and
`usp`) at the revisions validated by RZI. The application manifest still
selects a compatible `zephyr` revision itself.

RZI also owns temporary compatibility patches for its pinned dependencies.
After `west update`, apply them from the workspace root:

```sh
west patch -sm rzi clean
west patch -sm rzi apply --roll-back
```

Build wrappers may run these commands automatically. See
[`doc/west-patch.md`](doc/west-patch.md) for the ownership model and warnings.

Projects defined in the application manifest take precedence over imported
ones, so an application can override any imported version. To select another
backend, blocklist the USP projects on the import and declare that backend's
dependency set instead.

Application CMake remains a normal Zephyr application:

```cmake
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(customer_app)
target_sources(app PRIVATE src/main.c)
```

Applications include only the public API:

```c
#include <rzi/lorawan.h>
```

RZI owns USP initialization, modem serialization, callback translation, and
event queuing. Zephyr owns board hardware descriptions, USP owns its driver
bindings, and the customer application owns credentials, region selection,
product behavior, and its uplink schedule.

The public API is implemented by a backend-independent service layer. Backend
selection is a Kconfig choice, so another implementation can be added under
`src/lorawan/backends/` without changing applications or the public headers.

## Configuration

Enable RZI LoRaWAN in the application `prj.conf`:

```ini
CONFIG_GPIO=y
CONFIG_SPI=y
CONFIG_FLASH=y
CONFIG_RZI=y
CONFIG_RZI_LORAWAN=y
```

RZI does not export a Devicetree root or redefine board hardware. Applications
using a Zephyr revision that predates the required RAK4631 BSP or USP binding
support must carry that compatibility in their workspace or application until
the corresponding upstream changes are available.

See [`samples/lorawan/class_a`](samples/lorawan/class_a/README.rst) for a
minimal API example and [`samples/lorawan/at`](samples/lorawan/at/README.rst)
for a RAK RUI3-compatible AT command interface. Their overlays contain
zero-valued credentials and the sample-only compatibility required by the
currently pinned dependencies.

## Scope

Implemented now:

- asynchronous LoRaWAN initialization, OTAA join, leave, uplink, and events;
- USP/LBM backend isolation and serialized modem access;
- a transport-independent, extensible RUI3-compatible AT framework with an
  optional UART transport and LoRaWAN command package (AT, ATZ, ATR, VER,
  DEVEUI, APPEUI, APPKEY, BAND, NJM, NJS, CLASS, CFM, CFS, JOIN, SEND, RECV);
- flash persistence of AT parameters through Zephyr settings/NVS, using the
  board-defined `storage_partition` convention (no hardcoded addresses);
- standard Zephyr samples with Twister metadata.

ABP, power policy, diagnostics, FUOTA, and the Arduino/RUI C++ wrapper
remain planned services.

RZI follows the
[Zephyr module specification](https://docs.zephyrproject.org/latest/develop/modules.html)
and the layout conventions demonstrated by the
[Zephyr example application](https://github.com/zephyrproject-rtos/example-application).

## Style checks

All new and modified RZI code must follow the normative
[file and function naming conventions](doc/naming-conventions.md) in addition
to the Zephyr coding style. The repository carries a copy of Zephyr's
`.clang-format` (picked up automatically by editors and plain `clang-format`),
and `scripts/check-style.sh` runs clang-format plus Zephyr's `checkpatch.pl`
with Zephyr's own `.checkpatch.conf` rules.

A versioned pre-commit hook runs the checks on staged changes. Enable it
once per clone:

```bash
git config core.hooksPath .githooks
# checkpatch.pl is located via ZEPHYR_BASE, or once per clone:
git config rzi.zephyrbase <path-to-zephyr-checkout>
```

Bypass in an emergency with `git commit --no-verify`. Manual runs:

```bash
scripts/check-style.sh           # working tree, or HEAD when clean
scripts/check-style.sh --staged  # staged changes only
scripts/check-style.sh --fix     # apply clang-format in place
```
