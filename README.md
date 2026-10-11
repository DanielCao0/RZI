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

## Prerequisites

Install the Zephyr host tools, west, and the Zephyr SDK as described in the
[Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html).
Only the `arm-zephyr-eabi` toolchain is needed for the product boards.

RZI is validated with this stack, pinned in [`west.yml`](west.yml):

| Component | Version |
|---|---|
| Zephyr | v4.5.0-rc1 |
| Zephyr SDK | 1.0.1 |
| `usp_zephyr` | v1.2.1 |
| `usp` | v1.2.1 |

Other Zephyr revisions are not validated. The patches RZI applies to
`usp_zephyr` use Zephyr 4.5 APIs and do not build on Zephyr 4.4.

## Setup

Choose one of three workspace layouts:

| Layout | Manifest repository | Zephyr revision comes from | Use it for |
|---|---|---|---|
| [Standalone workspace](#standalone-workspace) | RZI | RZI | Evaluating RZI, running the samples, developing RZI |
| [Application imports RZI](#application-imports-rzi) | Your application | RZI | Product firmware that follows the stack RZI validated |
| [Application pins Zephyr](#application-pins-zephyr) | Your application | Your application | Products whose own manifest must hold the Zephyr pin |

Create the workspace in its own directory, not inside an existing Zephyr
workspace. Every layout imports all of Zephyr's modules, which is several
gigabytes; `west update --narrow -o=--filter=blob:none` fetches less history.
After setting up any layout, continue with
[after `west update`](#after-west-update).

### Standalone workspace

RZI is the manifest repository. Zephyr, `usp_zephyr`, and `usp` are fetched
at the revisions in [`west.yml`](west.yml).

```sh
mkdir rzi-workspace && cd rzi-workspace
git clone https://github.com/DanielCao0/RZI rzi
west init -l rzi
west update
```

### Application imports RZI

Your application repository is the manifest repository and imports RZI. RZI
brings Zephyr, `usp_zephyr`, and `usp`, so upgrading RZI upgrades the whole
validated stack. Production manifests pin a released RZI tag or commit, not
`main`:

```yaml
# app/west.yml
manifest:
  self:
    path: app

  projects:
    - name: rzi
      url: https://github.com/DanielCao0/RZI
      revision: <validated-rzi-tag-or-sha>
      path: rzi
      import: true
```

```sh
mkdir app-workspace && cd app-workspace
git clone <your-application-repository> app
west init -l app
west update
```

### Application pins Zephyr

Same as the previous layout, but the application lists `zephyr` before `rzi`.
West keeps the first definition of a project, so this Zephyr revision wins
and RZI's `zephyr` entry, including its import, is ignored. `usp_zephyr` and
`usp` still come from RZI:

```yaml
# app/west.yml
manifest:
  self:
    path: app

  projects:
    - name: zephyr
      url: https://github.com/zephyrproject-rtos/zephyr
      revision: v4.5.0-rc1
      import: true

    - name: rzi
      url: https://github.com/DanielCao0/RZI
      revision: <validated-rzi-tag-or-sha>
      path: rzi
      import: true
```

The only Zephyr revision RZI is validated against today is v4.5.0-rc1, the
same one RZI pins, so this layout currently moves the pin into your manifest
without changing it. Any other revision is untested. Do not override
`usp_zephyr` or `usp` in the application manifest: RZI's patches are
generated against the revisions it pins.

### After `west update`

Run these from the workspace root, inside the Python environment that has
west.

Install the Python packages that Zephyr and its modules need, including
`imgtool`, which sysbuild uses to sign the application for MCUboot. Repeat
this when the Zephyr revision changes:

```sh
west packages pip --install
```

Apply RZI's patches to `usp_zephyr` and `usp`. Repeat this after every
`west update`:

```sh
west patch -sm rzi clean
west patch -sm rzi apply --roll-back
```

The patches add the RAK3372 radio to `usp_zephyr`, adapt it to the pinned
Zephyr release, let `usp` export LoRaWAN session keys, and fix a header that
warns on every SX1262 build. `clean` discards
uncommitted changes in those two modules.
See [`doc/west-patch.md`](doc/west-patch.md).

Check that every project resolved to the expected revision:

```sh
west list zephyr usp_zephyr usp
```

## Using RZI in an application

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

Build from the workspace root of any of the layouts above:

```sh
west build -p always -b rzi_rak4631/nrf52840 --sysbuild rzi/samples/lorawan/periodical_uplink
```

For RAK3372, use `-b rzi_rak3372/stm32wle5xx`. Sysbuild combines MCUboot and
the signed application into `build/merged_<board>.hex`, for example
`build/merged_rzi_rak4631_nrf52840.hex`. Flash that file for the first
install, not `zephyr.hex`; see [`doc/boot.md`](doc/boot.md).

| Sample | Purpose |
|---|---|
| [`samples/lorawan/periodical_uplink`](samples/lorawan/periodical_uplink/README.rst) | OTAA join and periodic uplink, with a low-power variant |
| [`samples/lorawan/fuota`](samples/lorawan/fuota/README.rst) | ChirpStack-compatible FUOTA (`rzi_rak4631`) |
| [`samples/at`](samples/at/README.rst) | RUI3-compatible AT modem (LoRaWAN, P2P, radio tests) over USB CDC (`rzi_rak4631`) |
| [`samples/lora/tx_cw`](samples/lora/tx_cw/README.rst) | Continuous-wave carrier for RF measurements |
See [`samples/README.md`](samples/README.md) for board support and where to
start. The LoRaWAN overlays ship zero-valued credentials; replace them before
a radio test.

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
west.yml        Validated Zephyr and USP revisions
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
