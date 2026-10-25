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
| usp_zephyr | v1.2.1 |
| usp | v1.2.1 |

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
[After west update](#after-west-update).

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

### After west update

Run these from the workspace root, inside the Python environment that has
west.

Install the Python packages that Zephyr and its modules need, including
`imgtool`, which sysbuild uses to sign the application for MCUboot. Repeat
this when the Zephyr revision changes:

```sh
west packages pip --install
```

Apply RZI's patches to `zephyr`, `usp_zephyr`, and `usp`. Repeat this after every
`west update`:

```sh
west patch -sm rzi clean
west patch -sm rzi apply --roll-back
```

The patches add the RAK3372 radio to `usp_zephyr`, repair STM32WL STOP
resume in Zephyr, adapt the backend to the pinned Zephyr release, let `usp`
export LoRaWAN session keys, and fix a header that warns on every SX1262
build. `clean` discards uncommitted changes in those three modules.
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

## Supported boards

Choose the full target from this table for the `-b` argument to `west build`.
All targets are discovered automatically when RZI is present as a Zephyr
module; no extra `BOARD_ROOT` setting is required.

| Board | Build target | Console / AT connection | Firmware updates |
|---|---|---|---|
| [RAK4631](boards/rakwireless/rak4631/) | `rak4631/nrf52840/rzi` | USB CDC ACM for AT; the FUOTA and customer examples also select USB CDC for the console | Two internal slots; FUOTA supported |
| [RAK3172 / RAK3372](boards/rakwireless/rak3172/) | `rak3172/stm32wle5xx/rzi` | LPUART1, TX PA2 / RX PA3, through a USB-UART adapter or carrier | Single application slot; update over SWD |
| [RAK3372-F, using the RAK3172-F module](boards/rakwireless/rak3372_f/) | `rak3372_f/stm32wle5xx` | LPUART1, TX PA2 / RX PA3, through a USB-UART adapter or carrier | External MX25L400 holds the second slot; FUOTA supported |
| [RAK3362](boards/rakwireless/rak3362/) | `rak3362/nrf54l15/cpuapp` | UART20 through the base board's USB-UART converter | Two internal slots; FUOTA is not enabled in the current samples |

Physical UART connections use **115200 baud, 8N1**. For RAK4631, samples
without a USB-console overlay use UART1 on TX P0.16 / RX P0.15 for logs.

RAK4631 and standard RAK3172 extend upstream Zephyr boards. Keep the final
`/rzi` in their targets: it selects the RZI radio configuration, partitions,
and boot defaults. RAK3362 and RAK3372-F are independent board definitions
maintained in this repository. The standard RAK3172 target is for the
crystal-based STM32WLE5CC module; RAK3172-T needs a separate TCXO hardware
configuration, and the F module uses the dedicated `rak3372_f` target.

### Build and connect

After workspace setup and patch application, run these commands from the
workspace root. Each builds the same AT sample for a different board:

```sh
west build --sysbuild -b rak4631/nrf52840/rzi -d build/at-4631 rzi/samples/at
west build --sysbuild -b rak3172/stm32wle5xx/rzi -d build/at-3172 rzi/samples/at
west build --sysbuild -b rak3372_f/stm32wle5xx -d build/at-3372-f rzi/samples/at
west build --sysbuild -b rak3362/nrf54l15/cpuapp -d build/at-3362 rzi/samples/at
```

For an uplink application, replace `rzi/samples/at` with
`rzi/samples/lorawan/periodical_uplink` and use a separate build directory.
Set the sample's OTAA credentials in `app.overlay` or its selected board
overlay before joining a network. The AT sample instead accepts provisioning
through its [AT commands](samples/at/README.rst).

For initial installation, flash the **merged MCUboot + application HEX**
from the selected build directory over SWD. For example, the RAK4631 AT
image is `build/at-4631/merged_rak4631_nrf52840_rzi.hex`. The merged filename
contains the target with `/` replaced by `_`. After flashing, open the
connection shown in the table and send `AT` followed by a line ending.
See [boot and partition details](doc/boot.md) and the
[AT sample guide](samples/at/README.rst) for flashing and terminal setup.

## Samples

Build from the workspace root of any of the layouts above:

```sh
west build -p always -b rak4631/nrf52840/rzi --sysbuild rzi/samples/lorawan/periodical_uplink
```

For RAK3372, use `-b rak3172/stm32wle5xx/rzi`. Sysbuild combines MCUboot and
the signed application into `build/merged_<board>.hex`, for example
`build/merged_rak4631_nrf52840_rzi.hex`. Flash that file for the first
install, not `zephyr.hex`; see [`doc/boot.md`](doc/boot.md).

| Sample | Purpose |
|---|---|
| [`samples/lorawan/periodical_uplink`](samples/lorawan/periodical_uplink/README.rst) | OTAA join and periodic uplink, with a low-power variant |
| [`samples/lorawan/fuota`](samples/lorawan/fuota/README.rst) | ChirpStack-compatible FUOTA (`rak4631/nrf52840/rzi`, `rak3372_f`) |
| [`samples/at`](samples/at/README.rst) | RUI3-compatible AT modem (LoRaWAN, P2P, radio tests) over USB CDC (`rak4631/nrf52840/rzi`) or uart20 (`rak3362`) |
| [`samples/lora/tx_cw`](samples/lora/tx_cw/README.rst) | Continuous-wave carrier for RF measurements |

See [`samples/README.md`](samples/README.md) for board support and where to
start. Configure the selected sample's LoRaWAN credentials before a radio
test.

## Layout

```text
include/rzi/    Public C API, #include <rzi/...>
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
| [`doc/boot.md`](doc/boot.md) | Boards, MCUboot, merged image |
| [`doc/coding-standards.md`](doc/coding-standards.md) | Layout, naming, errors |

Public headers use Zephyr-style Doxygen:

```sh
scripts/generate-doxygen.sh
```

Open `doc/doxygen/html/index.html`. Rules: [`doc/api-docs.md`](doc/api-docs.md).

SBOMs follow the CRA / BSI TR-03183-2 expectations: SPDX 3.0.1 for every
release image (`west spdx`), plus a module SBOM in SPDX 3.0.1 and CycloneDX
1.6 for this repository and the backends it pins:

```sh
scripts/generate-sbom.sh
```

See [`doc/sbom.md`](doc/sbom.md) and, for vulnerability handling,
[`doc/security.md`](doc/security.md).

## Development

Contributions must follow [`doc/coding-standards.md`](doc/coding-standards.md)
and the Zephyr coding style. Enable the repository hooks once per clone:

```sh
git config core.hooksPath .githooks
```

`pre-commit` checks staged C style. `commit-msg` checks the message with
Zephyr gitlint, including the 75-character body line limit.

Check style before pushing:

```sh
scripts/check-style.sh           # working tree, or HEAD when clean
scripts/check-style.sh --staged  # staged changes only
scripts/check-style.sh --fix     # apply clang-format in place
```

CI runs those checks, twister on `native_sim`, sample builds for the
product boards, public-header compilation (C and C++), Doxygen, REUSE
compliance, and SPDX 3 / CycloneDX SBOM validation. Jobs and local
reproduction: [`doc/ci.md`](doc/ci.md).
