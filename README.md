# RZI

[![CI](https://github.com/DanielCao0/RZI/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/DanielCao0/RZI/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

RZI is a modular C SDK for building RAKWireless products on
[Zephyr](https://www.zephyrproject.org/). It provides stable, backend-neutral
APIs for LoRaWAN, raw LoRa/FSK, AT commands, persistent storage, and power
management.

RZI keeps protocol-stack details out of application code while retaining
Zephyr's native build system, device model, and hardware description. Product
firmware can therefore evolve its radio backend without changing its public
application interface.

- **Current version:** [0.2.0](include/rzi/version.h)
- **License:** [Apache-2.0](LICENSE)
- **Supported boards:** `rzi_rak4631/nrf52840` and
  `rzi_rak3372/stm32wle5xx`

## Key capabilities

- LoRaWAN API for OTAA, ABP, Class A/B/C, multicast, certification, MAC
  requests, and FUOTA, on Semtech USP / LoRa Basics Modem through `usp_zephyr`.
- Raw LoRa and FSK APIs for point-to-point communication and radio testing.
- RUI3-compatible AT command framework with pluggable transports.
- Namespaced nonvolatile storage and coordinated power management.
- Product board definitions, flash partitions, and sysbuild integration.

The public ABI lives exclusively under [`include/rzi/`](include/rzi/).
Backend types and third-party protocol headers are private implementation
details.

Registered AT commands and the gaps versus RUI3:
[`doc/at-command-compatibility.md`](doc/at-command-compatibility.md).

RZI follows the
[Zephyr module specification](https://docs.zephyrproject.org/latest/develop/modules.html).
Zephyr discovers this repository through [`zephyr/module.yml`](zephyr/module.yml).
Applications do not add RZI sources by hand.

## Getting started

Add RZI to the application `west.yml` and import its manifest. Production
revisions pin a released tag or commit, not `main`:

```yaml
manifest:
  projects:
    - name: rzi
      url: https://github.com/DanielCao0/RZI
      revision: <validated-rzi-tag-or-sha>
      path: rzi
      import: true
```

The import pulls `usp_zephyr` and `usp` at the revisions pinned in this
repository's [`west.yml`](west.yml). Product firmware uses that stack. The
application manifest still chooses its own Zephyr revision; leave the imported
USP projects at those pins.

After `west update`, apply RZI's compatibility patches from the workspace
root:

```sh
west patch -sm rzi clean
west patch -sm rzi apply --roll-back
```

See [`doc/west-patch.md`](doc/west-patch.md).

Application CMake is a normal Zephyr application:

```cmake
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(customer_app)
target_sources(app PRIVATE src/main.c)
```

Include only the public headers:

```c
#include <rzi/lorawan/lorawan.h>
```

Enable the services the product needs:

```ini
CONFIG_RZI=y
CONFIG_RZI_LORAWAN=y
```

RZI owns modem bring-up, serialization, callback translation, and event
queuing. Zephyr owns board hardware. The application owns credentials,
region, product behavior, and the uplink schedule.

Product images build with sysbuild and flash the merged image. See
[`doc/boot.md`](doc/boot.md).

## Samples

Build from a west workspace that already contains RZI:

```sh
west build -b rzi_rak4631/nrf52840 --sysbuild samples/lorawan/class_a
```

| Sample | Purpose |
|---|---|
| [`samples/lorawan/class_a`](samples/lorawan/class_a/README.rst) | Minimal OTAA join and periodic uplink |
| [`samples/at/lorawan`](samples/at/lorawan/README.rst) | RUI3-compatible LoRaWAN AT over USB CDC |
| [`samples/lorawan/fuota`](samples/lorawan/fuota/README.rst) | ChirpStack-compatible FUOTA (`rzi_rak4631`) |

Overlays ship zero-valued credentials. Replace them before a radio test.

## Layout

```text
include/rzi/    Public C API (`#include <rzi/...>`)
src/            Service implementations and private backends
boards/         Product boards and partition tables
zephyr/         Module metadata, Kconfig, CMake, and west patches
samples/        Buildable integration examples
tests/          Zephyr Twister test suites
doc/            Architecture, API contracts, and integration guides
scripts/        Style, documentation, and SBOM tooling
west.yml        Default USP dependency revisions
```

## Documentation

| Document | Contents |
|---|---|
| [`doc/README.md`](doc/README.md) | Catalog |
| [`doc/architecture.md`](doc/architecture.md) | Layers, ownership, roadmap |
| [`doc/lorawan-api.md`](doc/lorawan-api.md) | LoRaWAN C contract |
| [`doc/boot.md`](doc/boot.md) | Boards, MCUBoot, merged image |
| [`doc/coding-standards.md`](doc/coding-standards.md) | Layout, naming, errors |

Public headers use Zephyr-style Doxygen:

```bash
scripts/generate-doxygen.sh
```

Open `doc/doxygen/html/index.html`. Rules: [`doc/api-docs.md`](doc/api-docs.md).

Source-level SBOM (SPDX 2.3 and CycloneDX 1.6) for this module and the
backends it pins:

```bash
scripts/generate-sbom.sh
```

See [`doc/sbom.md`](doc/sbom.md).

## Development

Contributions must follow [`doc/coding-standards.md`](doc/coding-standards.md)
and the Zephyr coding style. Enable the repository hook once per clone:

```bash
git config core.hooksPath .githooks
```

```bash
scripts/check-style.sh           # working tree, or HEAD when clean
scripts/check-style.sh --staged  # staged changes only
scripts/check-style.sh --fix     # apply clang-format in place
```

CI runs those checks, twister on `native_sim`, sample builds for both
product boards, public-header compilation (C and C++), Doxygen, and SBOM
regeneration. Jobs and local reproduction: [`doc/ci.md`](doc/ci.md).

Architecture, ownership boundaries, and the service roadmap are documented in
[`doc/architecture.md`](doc/architecture.md). See
[`doc/README.md`](doc/README.md) for the complete documentation index.
