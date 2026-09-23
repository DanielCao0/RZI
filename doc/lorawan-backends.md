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

Existing `samples/lorawan/class_a`, `samples/at/lorawan`, and
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
live on product board `rzi_rak4631`. Samples no longer carry their own
overlay. Boot is in [boot.md](./boot.md).

From the west workspace root:

```bash
west build -p always --sysbuild \
  -b rzi_rak4631/nrf52840 \
  -d build/rzi-fuota-usp \
  rzi/samples/lorawan/fuota
```

For Class A or AT, replace the last path with
`rzi/samples/lorawan/class_a` or `rzi/samples/at/lorawan`.

Inside the container, same as the product app:

```bash
cd app
./scripts/container.sh build -p always --sysbuild \
  -b rzi_rak4631/nrf52840 \
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
runtime makes `start()` fail.

Board requirements:

- Product board `rzi_rak4631` defaults to USP `semtech,sx1262-new`
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
  -b rzi_rak4631/nrf52840 \
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

## 4. Common mistakes

- Adding only `CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR=y` to a USP `prj.conf`
  while leaving `CONFIG_LORA=n`: the choice conflicts or the setting is
  dropped.
- Enabling the Zephyr backend on `rzi_rak4631` without changing the
  `&lora` compatible: the USP driver and loramac-node fight over the same
  SX1262.
- Sharing one `-d` for both backends without `-p always`: CMake cache
  still has the old stack.
- Enabling `CONFIG_RZI_MCUBOOT` without `--sysbuild`: configuration
  fails.
- Flashing `zephyr.hex` as the whole-device image: no MCUBoot at 0x0.
- Built-in `CONFIG_LORAWAN_REGION_*` does not match
  `rzi_lorawan_set_region()`.
