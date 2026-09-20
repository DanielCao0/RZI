# RSUP API

Status: implemented

Public C contract for RSUP (RZI Slot Update Protocol) type 1: write a signed
MCUboot image into `image-1` over a Zephyr UART (USB CDC or hardware UART).
Type 0 (Arduino LLEXT sketch) is rejected.

See also: [error-codes.md](./error-codes.md), [boot.md](./boot.md),
[power.md](./power.md).

The header is `include/rzi/rsup/rsup.h`. Enable with `CONFIG_RZI_RSUP=y`
(requires `CONFIG_RZI_MCUBOOT_DUAL_SLOT`). Single-slot boards such as
`rzi_rak3372` cannot select it.

`<rzi/rsup/slot_update.h>` and `<rzi/rsup/usb_update.h>` are deprecated
wrappers around the same symbols.

## Wire protocol

| Item | Value |
|---|---|
| Magic | ASCII `RSUP` (`RSUP_MAGIC`) |
| Type 0 | Sketch payload; RZI returns `-RZI_ERR_INVALID` |
| Type 1 | Signed MCUboot slot1 image (`RSUP_TYPE_SLOT1`) |
| Dedicated-update flag | `RZI_RSUP_GPREGRET` (`0xA5`) |

The UART comes from `chosen rzi,rsup-uart`, else `zephyr,console`. A 1200 bps
touch (when the driver reports line control) writes the GPREGRET flag and
resets. The next boot calls `rzi_rsup_run()`.

## Functions

| Function | Role |
|---|---|
| `rzi_rsup_uart()` | Device selected by the chosen node |
| `rzi_rsup_requested()` | Consume a pending GPREGRET request |
| `rzi_rsup_run(uart)` | Blocking session; success reboots and does not return |
| `rzi_rsup_arm_reboot()` | Write GPREGRET and cold-reset |

`rzi_rsup_run()` takes `RZI_POWER_BLOCK_UPDATE` for the whole session when
`CONFIG_RZI_POWER_AUTO_SERVICE_BLOCK=y`.

RZI does not modify MCUBoot source. First flash and brick recovery still use
`merged.hex`; see [boot.md](./boot.md).
