# FUOTA

Status: implemented

RZI owns update state, image read, and optional MCUboot installation. Clock
Synchronization, Remote Multicast Setup, and Fragmentation stay in the
selected backend. Independent GenAppKey multicast is complete only on
USP/LBM.

See also: [lorawan-backends.md](./lorawan-backends.md),
[lorawan-backend-zephyr.md](./lorawan-backend-zephyr.md),
[boot.md](./boot.md).

`CONFIG_RZI_LORAWAN_FUOTA` depends on `CONFIG_RZI_MCUBOOT_DUAL_SLOT` when
MCUBoot is enabled, so `rzi_rak3372` cannot select it.

## Device-side flow

The application owns OTAA join and periodic uplinks, and observes progress
with `rzi_fuota_register_callbacks()`. Protocol and reassembly run
automatically when `CONFIG_RZI_LORAWAN_FUOTA=y`:

```text
OTAA join
  -> rzi_fuota_start() (called automatically from lorawan_start)
  -> start ALCSync and request MAC DeviceTime
  -> periodic uplink (ChirpStack advances unicast steps from this)
  -> FPort 202 / 200 / 201 handled transparently by the backend
  -> Class C multicast receives fragments
  -> record the image header; state becomes COMPLETE
  -> optional rzi_fuota_apply() writes the MCUboot secondary slot and reboots
```

OTAA `application_key` is the LoRaWAN 1.0.x **GenAppKey** and must match
the device Gen App Key in ChirpStack. `network_key` is the 1.0.x AppKey.

## ChirpStack configuration

1. Device profile → Application layer, enable:
   - Application Layer Clock Synchronization **v1**
   - Remote Multicast Setup **v1**
   - Fragmented Data Block Transport **v1**
2. Set Expected uplink interval to 5–10 seconds. ChirpStack checks unicast
   step completion on that interval; a longer interval looks like a stall
   in Multicast Setup.
3. Fill the device OTAA keys: AppKey and Gen App Key.
4. Create a FUOTA deployment:
   - Multicast group type: **Class C**
   - Let ChirpStack compute fragment size and multicast timeout
   - Configure fragmentation redundancy (for example 10–20%)
5. Upload the file to send. For protocol bring-up use
   `samples/lorawan/fuota/test-payload/rzi-fuota-test.bin` (4096 bytes,
   ASCII `RZI1` at the start). That is not bootable firmware. For MCUboot
   installation, switch to a signed image.

Device firmware must select the same package versions:

```
CONFIG_RZI_LORAWAN_FUOTA=y
CONFIG_LORA_BASICS_MODEM_FUOTA_V1=y
```

Optional RZI knobs (see `zephyr/Kconfig.lorawan`):

- `CONFIG_RZI_LORAWAN_FUOTA_KEEPALIVE_INTERVAL_S` — automatic one-byte
  unicast keepalive. Default `0` (off); the application should send its
  own periodic uplinks.
- `CONFIG_RZI_LORAWAN_FUOTA_KEEPALIVE_PORT` — LoRaWAN port used by the
  keepalive uplink when the interval is non-zero.
- `CONFIG_RZI_LORAWAN_FUOTA_AUTO_APPLY` — copy a completed image to slot1
  and reboot. Requires `CONFIG_IMG_MANAGER`.
- `CONFIG_RZI_LORAWAN_FUOTA_HW_VERSION` — 32-bit hardware version reported
  to FMP.

If the ChirpStack device profile uses v2, switch to
`CONFIG_LORA_BASICS_MODEM_FUOTA_V2=y`.

ChirpStack core FUOTA does **not** use the Firmware Management Protocol
(TS006). LBM still builds FMP by default; it only runs when the network
server sends FPort 203.

## Image capacity

LBM writes the reassembled image into the context partition
`CONTEXT_FUOTA` (default offset 4096). Usable size is limited by that
partition and by `FRAG_MAX_NB * FRAG_MAX_SIZE`.

The default sample configuration is about 200 × 100 = 20 KB. That is
enough to verify the protocol and cannot hold a full nRF52840 application.
A whole-firmware upgrade needs:

1. A larger `CONFIG_LORA_BASICS_MODEM_FUOTA_MAX_NB_OF_FRAGMENTS`
2. A larger `lora-basics-modem-context-partition`, or
   `CONFIG_RZI_MCUBOOT` so `rzi_fuota_apply()` copies the image to slot1
   (see [boot.md](./boot.md))

Without `CONFIG_IMG_MANAGER`, `rzi_fuota_apply()` returns `-RZI_ERR_NOT_SUPPORTED`.
Applications can still extract the image with `rzi_fuota_read_image()`.

`rzi_rak3372` is single-slot (no `image-1`), so
`CONFIG_RZI_LORAWAN_FUOTA` cannot be selected.

## Public API

See `include/rzi/lorawan/fuota.h`:

- `rzi_fuota_register_callbacks()`
- `rzi_fuota_start()`
- `rzi_fuota_get_status()`
- `rzi_fuota_set_expected_size()` (optional override; the backend reports
  reassembly length, and MCUboot images are measured automatically)
- `rzi_fuota_read_image()`
- `rzi_fuota_apply()`

Reference sample: `samples/lorawan/fuota`.
