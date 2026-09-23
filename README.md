# RZI

[![CI](https://github.com/DanielCao0/RZI/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/DanielCao0/RZI/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

RZI is a Zephyr module for RAK product firmware. Applications call a stable C
API; protocol-stack types stay private, and board hardware stays in Zephyr
plus the RZI product boards.

**Version 0.2.0** · Apache-2.0 · boards `rzi_rak4631/nrf52840` and
`rzi_rak3372/stm32wle5xx`

The public ABI is `include/rzi/`. Architecture, ownership, and the service
roadmap are in [`doc/architecture.md`](doc/architecture.md). The document
catalog is [`doc/README.md`](doc/README.md).

## Status

| Area | State |
|---|---|
| LoRaWAN C API (`include/rzi/lorawan/`) | OTAA join/send, classes and extras as advertised by the backend, multicast, certification, FUOTA |
| LoRaWAN backends | Default Semtech USP / LoRa Basics Modem; optional Zephyr `lorawan_*` adapter |
| Raw LoRa / FSK (`include/rzi/lora/lora.h`) | P2P and radio-test C API |
| AT | RUI3-compatible CLI over an optional UART adapter; not a full RUI3 firmware |
| Storage, power | Implemented |
| Diagnostics | Planned |
| Arduino / RUI C++ | OTAA subset lives in Arduino Core for Zephyr, not this repository |

The public join API accepts OTAA and ABP. The default USP backend implements
OTAA. The Zephyr backend also implements ABP. FUOTA needs a dual-slot
board (`rzi_rak4631`); `rzi_rak3372` is single-slot.

Registered AT commands and the gaps versus RUI3:
[`doc/at-command-compatibility.md`](doc/at-command-compatibility.md).

RZI follows the
[Zephyr module specification](https://docs.zephyrproject.org/latest/develop/modules.html).
Zephyr discovers this repository through [`zephyr/module.yml`](zephyr/module.yml).
Applications do not add RZI sources by hand.

## Integration

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

The import pulls the default USP dependencies (`usp_zephyr` and `usp`) at the
revisions in this repository's [`west.yml`](west.yml). The application
manifest still chooses its own Zephyr revision. Projects defined there take
precedence, so an application can override any imported version or blocklist
the USP projects and declare another backend.

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
region, product behavior, and the uplink schedule. Backend selection is a
Kconfig choice under `src/lorawan/backend/`; applications and public headers
do not change when a backend is added.

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
include/rzi/   Public C ABI (`#include <rzi/...>`)
src/           Per-service implementation
zephyr/        Module metadata, Kconfig, CMake, west patches
boards/        Product boards and partition tables
samples/       Buildable examples
tests/         Twister suites
doc/           Architecture and API notes
scripts/       Style, Doxygen, and SBOM helpers
west.yml       Default USP backend pins
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

New code follows [`doc/coding-standards.md`](doc/coding-standards.md) and the
Zephyr coding style. Enable the versioned hook once per clone:

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
