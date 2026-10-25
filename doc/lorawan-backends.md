# LoRaWAN backends

Status: implemented

Build-time Kconfig switch between USP and the Zephyr `lorawan_*` adapter.

See also: [lorawan-backend-zephyr.md](./lorawan-backend-zephyr.md),
[fuota.md](./fuota.md), [lorawan-api.md](./lorawan-api.md).

RZI applications call only `rzi_lorawan_*`. The protocol stack is selected
at **build time** with a Kconfig `choice`. One firmware image has one
backend. It cannot be switched at runtime.

```text
CONFIG_RZI_LORAWAN_BACKEND_USP       Default: USP + LoRa Basics Modem
CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR    Zephyr <zephyr/lorawan/lorawan.h>, loramac-node underneath
CONFIG_RZI_LORAWAN_BACKEND_TEST      Unit tests only
```

Planned `CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR_LBM` is not implemented. Do not
confuse it with the Zephyr backend above.

The two stacks are exclusive: USP requires `CONFIG_LORA=n` and
`CONFIG_LORAWAN=n`. The Zephyr backend enables both and drives the same
SX1262 through loramac-node. Do not let both drive the radio.

Application code does not change. What changes is **Kconfig, the board
overlay, and a separate build directory**. After switching backends use
`-p always` or a different `-d` so the old CMake cache is not reused.

## 1. USP / LBM (default, product path)

Existing `samples/lorawan/periodical_uplink`, `samples/at`, and
`samples/lorawan/fuota` use this path.

Required configuration:

```conf
CONFIG_RZI=y
CONFIG_RZI_LORAWAN=y
CONFIG_RZI_LORAWAN_BACKEND_USP=y
CONFIG_LORA=n
CONFIG_LORAWAN=n
```

`BACKEND_USP` is the choice default and may be omitted. Keep
`CONFIG_LORA=n` or Kconfig will turn USP options off.

Hardware and the USP SX1262 compatibility layer (`semtech,sx1262-new`)
live on product board `rak4631/nrf52840/rzi`. Samples no longer carry their own
overlay. Boot is in [boot.md](./boot.md).

From the west workspace root:

```bash
west build -p always --sysbuild \
  -b rak4631/nrf52840/rzi \
  -d build/rzi-fuota-usp \
  rzi/samples/lorawan/fuota
```

For Class A or AT, replace the last path with
`rzi/samples/lorawan/periodical_uplink` or `rzi/samples/at`.

Inside the container, same as the product app:

```bash
cd app
./scripts/container.sh build -p always --sysbuild \
  -b rak4631/nrf52840/rzi \
  -d /workdir/build/rzi-fuota-usp \
  /workdir/rzi/samples/lorawan/fuota
```

Flash the merged image:

```bash
cd app
./scripts/flash-rak4631.sh ../build/rzi-fuota-usp/merged.hex
```

Confirm USP is selected:

```bash
grep CONFIG_RZI_LORAWAN_BACKEND build/rzi-fuota-usp/zephyr/.config
```

Expect `CONFIG_RZI_LORAWAN_BACKEND_USP=y`.

## 2. Zephyr `lorawan_*` + loramac-node

This path uses the Zephyr public LoRaWAN API. The lower layer must be
**loramac-node**. Do not select `LORA_MODULE_BACKEND_NATIVE` or
`LORA_MODULE_BACKEND_LORA_BASICS_MODEM`.

Required configuration:

```conf
CONFIG_RZI=y
CONFIG_RZI_LORAWAN=y
CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR=y
CONFIG_LORA_MODULE_BACKEND_LORAMAC_NODE=y
CONFIG_LORAWAN_REGION_EU868=y
```

Do not also write `CONFIG_LORA=n` / `CONFIG_LORAWAN=n`. `BACKEND_ZEPHYR`
enables them. `CONFIG_LORAWAN_REGION_*` must match
`rzi_lorawan_set_region()`. Building only EU868 and setting US915 at
runtime makes `rzi_lorawan_init()` fail.

Board requirements:

- Product board `rak4631/nrf52840/rzi` defaults to USP `semtech,sx1262-new`
- The Zephyr backend needs an overlay that sets `&lora` back to official
  `semtech,sx1262` and restores properties that compatible requires
- USB console and `zephyr,user` keys stay in the application overlay

`zephyr.conf`:

```conf
CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR=y
CONFIG_LORA_MODULE_BACKEND_LORAMAC_NODE=y
CONFIG_LORAWAN_REGION_EU868=y
```

```bash
west build -p always --sysbuild \
  -b rak4631/nrf52840/rzi \
  -d build/rzi-fuota-zephyr \
  rzi/samples/lorawan/fuota \
  -- \
  -DEXTRA_CONF_FILE=zephyr.conf \
  -DDTC_OVERLAY_FILE=<your-zephyr-radio.overlay>
```

Confirm:

```bash
grep -E 'CONFIG_RZI_LORAWAN_BACKEND|CONFIG_LORA_MODULE_BACKEND' \
  build/rzi-fuota-zephyr/rzi_lorawan_fuota/zephyr/.config
```

Expect:

```text
CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR=y
CONFIG_LORA_MODULE_BACKEND_LORAMAC_NODE=y
# CONFIG_LORA_MODULE_BACKEND_LORA_BASICS_MODEM is not set
# CONFIG_LORA_MODULE_BACKEND_NATIVE is not set
```

`lorawan_backend_zephyr.c` has a `BUILD_ASSERT`: anything other than
loramac-node fails the build.

Contract tests on the native simulator (no board flash):

```bash
ZEPHYR_TOOLCHAIN_VARIANT=host west build -p always \
  -b native_sim \
  -d build/rzi-lorawan-backend-zephyr \
  rzi/tests/lorawan/backend/zephyr

ZEPHYR_TOOLCHAIN_VARIANT=host west build \
  -d build/rzi-lorawan-backend-zephyr \
  -t run
```

## 3. Choosing FUOTA

When `CONFIG_RZI_LORAWAN_FUOTA=y`, the RZI coordination layer (state,
callbacks, image read, optional `rzi_fuota_apply()`) is compiled on both
paths. Protocol packages stay in each stack.

| | USP / LBM | Zephyr + loramac-node |
| --- | --- | --- |
| Clock Sync / RMS / Fragmentation | LBM packages, complete events | Zephyr services, incomplete events |
| Independent GenAppKey multicast | Supported | Join cannot take a separate GenAppKey |
| ChirpStack bring-up | Existing FUOTA sample | Not yet brought up on this path on RAK4631 |
| Recommendation | Product FUOTA | Validate the RZI contract |

For ChirpStack FUOTA, use the USP commands in section 1. On the Zephyr
path, GenAppKey must equal AppKey, and you must also enable
`CONFIG_LORAWAN_SERVICES`, `CONFIG_LORAWAN_APP_CLOCK_SYNC`, and
`CONFIG_LORAWAN_FRAG_TRANSPORT`. Multicast also needs
`CONFIG_LORAWAN_REMOTE_MULTICAST` (depends on NVM settings).
`CONFIG_RZI_LORAWAN_FUOTA` implies those options and will not force them
on when dependencies are missing.

## 4. Backend contract

USP / LoRa Basics Modem is the reference. The private contract in
`src/lorawan/backend/lorawan_backend.h` is written in LBM terms, and the Zephyr
backend exists to prove a second stack can sit behind it. A backend:

- keeps sending Join-Requests after `join()`, reporting each failure with
  `JOIN_FAILED`, until it joins or `leave()` is called. The service counts
  attempts and calls `leave()`; it never re-issues `join()`. The Zephyr
  backend emulates this with a delayed work item;
- owns the protocol state: joined state, current data rate, busy state
  (`is_busy` is mandatory) and the enabled uplink data rates for the region
  (`get_enabled_data_rates`). The service keeps no shadow copies;
- reports stack-side event loss as `ERROR` with `-RZI_ERR_OVERFLOW`;
- sets a capability bit exactly when the matching ops table is non-NULL.
  `rzi_lorawan_init()` asserts this. USP derives class B, class C, and
  multicast from `CONFIG_LORA_BASICS_MODEM_CLASS_B`, `_CLASS_C`, and
  `_MULTICAST`;
- with `RZI_LORAWAN_CAP_CHANNEL` (no ops table), applies
  `join_config.channel_mask` at `join()` and restores every channel when
  `channel_mask_set` is false. The core owns and persists the mask. USP calls
  `smtc_modem_set_channel_mask()` from the `usp/0003` patch; LBM ANDs it into
  every default channel mask, and the ADR back-off re-enables every channel.
  Zephyr sets loramac-node's default mask and current mask
  (`MIB_CHANNELS_DEFAULT_MASK`, `MIB_CHANNELS_MASK`), because a join resets the
  current mask to the default mask. Its ADR back-off returns to the application
  mask.

The network ops table only carries controls LBM has (ADR profile, NbTrans,
ADR ACK, join data rates, duty-cycle status, LBT, CSMA, lost-connection
counter, no-downlink threshold, crystal error, empty uplink). RUI3 parameters
LBM has no setter for are answered `-RZI_ERR_NOT_SUPPORTED` by the service
without a backend call.

### USP platform layer

`src/usp/` (`CONFIG_RZI_USP`, selected by both USP backends) is shared by the
LoRaWAN backend and the raw LoRa backend. It is backend support, not a feature
module: it has no public header under `include/rzi/`, and `src/usp/` is on the
library include path so backends write `#include "usp_platform.h"`.

- `rzi_usp_init()` runs the platform, RAC, and modem init once;
- `rzi_usp_lock()` / `rzi_usp_unlock()` take the recursive `rac_api_mutex`;
  unlock wakes the USP thread so queued requests run promptly;
- `rzi_usp_set_event_handler()` routes LBM events to the LoRaWAN backend and
  replays a modem RESET seen before the handler was installed;
- `rzi_usp_result()` is the single LBM-to-`RZI_ERR_*` mapping.

The LoRaWAN-only LBM internals stay with the LoRaWAN backend.
`src/lorawan/backend/usp/lorawan_backend_usp_lbm.h` declares the few
LBM-internal functions it calls. `lbm_checks.cmake` in that directory checks
their prototypes against the LBM sources and fails the build if LBM changes
them. It also stops the build when the `usp/0003` patch
(`smtc_modem_set_channel_mask()`) is missing.

## 5. Common mistakes

- Adding only `CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR=y` to a USP `prj.conf`
  while leaving `CONFIG_LORA=n`: the choice conflicts or the setting is
  dropped.
- Enabling the Zephyr backend on `rak4631/nrf52840/rzi` without changing the
  `&lora` compatible: the USP driver and loramac-node fight over the same
  SX1262.
- Sharing one `-d` for both backends without `-p always`: CMake cache
  still has the old stack.
- Enabling `CONFIG_RZI_MCUBOOT` without `--sysbuild`: configuration
  fails.
- Flashing `zephyr.hex` as the whole-device image: no MCUBoot at 0x0.
- Built-in `CONFIG_LORAWAN_REGION_*` does not match
  `rzi_lorawan_set_region()`.
