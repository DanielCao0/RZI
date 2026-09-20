# Raw LoRa API

Status: implemented

Public C contract for P2P LoRa, P2P FSK, and radio certification tests.
The ABI is `include/rzi/lora/lora.h` (group 0.4.0).

See also: [error-codes.md](./error-codes.md),
[at-command-compatibility.md](./at-command-compatibility.md),
[lorawan-api.md](./lorawan-api.md).

## Purpose

`rzi_lora_*` owns raw radio operation that is not a LoRaWAN session.
Applications and the AT package share this API. Public types do not expose
USP, LoRa Basics Modem, or Zephyr LoRa driver types.

A return value of zero from `rzi_lora_send()` or `rzi_lora_receive()` means
the request was accepted. Completion is `tx_done` / `rx_done`.

## Backends

| Kconfig | Radio |
|---|---|
| `CONFIG_RZI_LORA_BACKEND_USP` | LoRa Basics Modem test API (product USP images) |
| `CONFIG_RZI_LORA_BACKEND_ZEPHYR` | Zephyr `<zephyr/drivers/lora.h>` |
| `CONFIG_RZI_LORA_BACKEND_TEST` | Software loopback |

USP P2P and LoRaWAN cannot use the radio at the same time. Switch with
`AT+NWM` (the production path reboots). Encryption is XOR of the payload
with the stored key and IV, not RUI3 AES-CTR.

## Limits

- Spreading factor 5 through 12, TX power 5 through 22 dBm.
- Payload up to `RZI_LORA_MAX_PAYLOAD` (255 bytes).
- `timeout_ms` 0 stops receive, 65535 receives continuously.
- Radio tests that the selected backend does not implement return
  `-RZI_ERR_NOT_SUPPORTED`. Backend and driver codes are mapped at the
  boundary and never forwarded as POSIX errno.
