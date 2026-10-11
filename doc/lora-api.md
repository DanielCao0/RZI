# Raw LoRa API

Status: implemented

Public C contract for P2P LoRa, P2P FSK, and radio certification tests.
The ABI is `include/rzi/lora/lora.h` (group 0.5.0).

See also: [error-codes.md](./error-codes.md),
[at-command-compatibility.md](./at-command-compatibility.md),
[lorawan-api.md](./lorawan-api.md).

## Purpose

`rzi_lora_*` owns raw radio operation that is not a LoRaWAN session.
Applications and the AT package share this API. Public types do not expose
USP, LoRa Basics Modem, or Zephyr LoRa driver types.

A return value of zero from `rzi_lora_send()` or `rzi_lora_receive()` means
the request was accepted. Completion is `tx_done` / `rx_done`.

With the USP backend, `tx_done` fires when the packet has left the radio,
and each received packet produces one `rx_done`. Both are delivered from
`rzi_lora_poll()` in the calling thread, so a client that waits for them
must call `rzi_lora_poll()` periodically (the AT package does this from its
process loop).

`rzi_lora_test_rssi()` starts a measurement at the configured frequency;
`rzi_lora_test_get_rssi()` returns `-RZI_ERR_BUSY` until the result is
ready.

## Backends

| Kconfig | Radio |
|---|---|
| `CONFIG_RZI_LORA_BACKEND_USP` | USP radio access controller (RAC); LBM test mode for certification tests |
| `CONFIG_RZI_LORA_BACKEND_ZEPHYR` | Zephyr `<zephyr/drivers/lora.h>` |
| `CONFIG_RZI_LORA_BACKEND_TEST` | Software loopback |

With USP, P2P send and receive run as RAC transactions at
`RAC_LOW_PRIORITY` (TX) and `RAC_VERY_LOW_PRIORITY` (RX), below every LoRaWAN
radio task. They share the radio with a running LoRaWAN session: LoRaWAN
preempts them, an aborted transmission completes with `-RZI_ERR_BUSY`, and
continuous or timed receive is rescheduled after the preemption until its
deadline. Certification tests (`rzi_lora_test_*`, CW, RSSI) use LoRa Basics
Modem test mode, which suspends LoRaWAN until `rzi_lora_stop()` or the next
P2P request. The FSK format in test mode is fixed by LBM (24-bit sync word
`C1 94 C1`, 40-bit preamble, inverted CCITT CRC, whitening).

The AT package still separates the modes with `AT+NWM` (the production path
reboots). Encryption is XOR of the payload with the stored key and IV, not
RUI3 AES-CTR.

## Limits

- Spreading factor 5 through 12, TX power 5 through 22 dBm.
- Payload up to `RZI_LORA_MAX_PAYLOAD` (255 bytes).
- `timeout_ms` 0 stops receive, 65535 receives continuously.
- Radio tests that the selected backend does not implement return
  `-RZI_ERR_NOT_SUPPORTED`. Backend and driver codes are mapped at the
  boundary and never forwarded as POSIX errno.
