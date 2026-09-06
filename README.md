# RZI

RZI is an independent Zephyr module that exposes RAK-oriented C APIs while
keeping protocol-stack and board integration private. The first implemented
service is LoRaWAN over Semtech USP and LoRa Basics Modem.

## Repository layout

```text
rzi/
├── zephyr/
│   ├── module.yml       Zephyr module metadata
│   ├── Kconfig          RZI feature configuration
│   └── CMakeLists.txt   Zephyr library integration
├── include/rzi/         Public C API
├── src/
│   └── lorawan/
│       ├── lorawan.c          Backend-independent service
│       ├── backend.h          Private backend contract
│       └── backends/usp.c     Semtech USP implementation
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
minimal API example. Its overlay contains zero-valued credentials and the
sample-only compatibility required by the currently pinned dependencies.

## Scope

Implemented now:

- asynchronous LoRaWAN initialization, OTAA join, leave, uplink, and events;
- USP/LBM backend isolation and serialized modem access;
- a standard Zephyr sample with Twister metadata.

AT commands, general NVM/configuration, power policy, diagnostics, FUOTA, and
the Arduino/RUI C++ wrapper remain planned services.

RZI follows the
[Zephyr module specification](https://docs.zephyrproject.org/latest/develop/modules.html)
and the layout conventions demonstrated by the
[Zephyr example application](https://github.com/zephyrproject-rtos/example-application).
