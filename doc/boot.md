# Device boot

Status: implemented

RZI does not implement a bootloader. MCUBoot source remains a west-fetched
module. RZI owns how customers enable it, how each board is partitioned,
how to flash, and how FUOTA lands in a slot.

See also: [fuota.md](./fuota.md).

## Contract (every product board)

1. Target an RZI product board. Do not use upstream `rak4631` / `rak3172`
   as the customer entry.
2. Enable `CONFIG_RZI_MCUBOOT=y`. Official samples and the product app
   already do.
3. Build with **sysbuild**. Configuration fails without it, so an
   application-only image is not flashed with no bootloader at 0x0.
4. First flash and recovery: flash the **merged image**
   (`merged_<board>.hex`) from the build directory, not `zephyr.hex`.
5. Later upgrades on dual-slot boards: write a signed MCUBoot image into
   **slot1** (`image-1`), then use `rzi_fuota_apply()` (LoRaWAN FUOTA).
   RZI uses partition names, not a board-specific absolute address.
   Single-slot boards (3372) have no `image-1`, so they do not support
   FUOTA. Reflash the merged image.

Partition tables live on the product board, not in RZI module Kconfig.
Adding a board only adds that board's DTS and flash notes.

| Board | Full name | west target | Status |
|---|---|---|---|
| `rzi_rak4631` | RZI RAK4631 | `rzi_rak4631/nrf52840` | MCUBoot and LoRaWAN samples wired |
| `rzi_rak3372` | RZI RAK3372 | `rzi_rak3372/stm32wle5xx` | Single-slot MCUBoot; on-chip SUBGHZ (USP patch); no FUOTA |
| `rzi_rak3372_f` | RZI RAK3372-F | `rzi_rak3372_f/stm32wle5xx` | Overwrite-only MCUBoot; image-1 on the on-module MX25L400; FUOTA |
| `rzi_rak3362` | RZI RAK3362 | `rzi_rak3362/nrf54l15/cpuapp` | Overwrite-only MCUBoot; SX1262 on SPI22; console and AT on uart20 |

nRF52840 (RAK4631) partitions are shared by native Zephyr and
ArduinoCore-zephyr:

| Partition | Address | Size | Use |
|---|---|---|---|
| `mcuboot` | `0x00000` | 48 KB | Sole bootloader |
| `image-0` | `0x0c000` | 304 KB | RZI app or Arduino loader |
| `image-1` | `0x58000` | 304 KB | FUOTA / slot swap |
| `user` | `0xa4000` | 336 KB | Arduino LLEXT sketch |
| `storage` | `0xf8000` | 32 KB | settings |

Changing mode only replaces the signed image in slot0. MCUBoot stays.
Native applications ignore the `user` partition.

STM32WLE5 (RAK3372, 256 KB) is single-slot and has no `image-1` / `user`:

| Partition | Address | Size | Use |
|---|---|---|---|
| `mcuboot` | `0x00000` | 32 KB | Sole bootloader |
| `image-0` | `0x08000` | 216 KB | RZI application |
| `storage` | `0x3e000` | 8 KB | settings |

nRF54L15 (RAK3362, 1524 KB RRAM) uses the same dual-slot layout as the
upstream nRF54L15 boards without FLPR:

| Partition | Address | Size | Use |
|---|---|---|---|
| `mcuboot` | `0x000000` | 64 KB | Sole bootloader |
| `image-0` | `0x010000` | 712 KB | RZI application |
| `image-1` | `0x0c2000` | 712 KB | FUOTA / overwrite |
| `storage` | `0x174000` | 36 KB | settings |

RAK3372-F uses a larger boot partition than RAK3372, because MCUboot
must drive the MX25L400, and adds the secondary slot on that NOR:

| Partition | Device | Address | Size | Use |
|---|---|---|---|---|
| `mcuboot` | internal | `0x00000` | 48 KB | Sole bootloader (includes the SPI NOR driver) |
| `image-0` | internal | `0x0c000` | 200 KB | RZI application |
| `storage` | internal | `0x3e000` | 8 KB | settings |
| `image-1` | MX25L400 | `0x00000` | 200 KB | FUOTA / overwrite |

Sysbuild mode comes from the board `Kconfig.sysbuild`: 4631, 3362, and
3372-F are overwrite-only; 3372 is `MCUBOOT_MODE_SINGLE_APP`. The radio is the
STM32WLE5 on-chip SUBGHZ (`st,stm32wl-subghz-radio`). The USP driver
needs `zephyr/patches/usp_zephyr/0005-stm32wl-subghz-radio.patch` and
`0008-stm32wl-tcxo-reset-and-irq.patch`.

This is **not** compatible with RUI3 / factory Arduino boot: no `AT+BOOT`,
`nrfutil`, or UF2. Moving to RZI is a one-time SWD boot change, the same
class as RAK swapping 4631 versus 4631-R boot.

Development builds use the MCUBoot default key. Production must switch to
your own signing key (this repository does not yet wire CI signing).

## This round: RZI RAK4631

```bash
west build -p always --sysbuild \
  -b rzi_rak4631/nrf52840 \
  -d build/rzi-periodical-uplink \
  rzi/samples/lorawan/periodical_uplink
```

Artifact: `build/rzi-periodical-uplink/merged_rzi_rak4631_nrf52840.hex` (boot + signed
application).

Product app (in `app/`):

```bash
./scripts/container.sh build
./scripts/flash-rak4631.sh
```

Default flash is `build/app/merged.hex`.

Keys go in the application overlay (`app.overlay` or
`boards/rzi_rak4631_nrf52840.overlay`). Do not edit board DTS. Radio,
partitions, and USP SX1262 compatibility are already on `rzi_rak4631`.

## RZI RAK3372 (single slot)

```bash
west build -p always --sysbuild \
  -b rzi_rak3372/stm32wle5xx \
  -d build/rzi-periodical-uplink-3372 \
  rzi/samples/lorawan/periodical_uplink
```

The artifact is `build/rzi-periodical-uplink-3372/merged_rzi_rak3372_stm32wle5xx.hex`.
There is no slot1, so FUOTA is not available.

## RZI RAK3372-F (external secondary slot)

```bash
west build -p always --sysbuild \
  -b rzi_rak3372_f/stm32wle5xx \
  -d build/rzi-fuota-3372f \
  rzi/samples/lorawan/fuota
```

The artifact is `build/rzi-fuota-3372f/merged_rzi_rak3372_f_stm32wle5xx.hex`.
FUOTA writes the signed image to `image-1` on the MX25L400. MCUboot copies
it over `image-0` on the next boot.
