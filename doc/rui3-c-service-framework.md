# RUI3 C service framework mapping

## Purpose

RZI reserves stable ownership boundaries for the complete RUI3 LoRa service
surface before implementing every feature. The reference is the RUI3 C service
and C AT implementation, not `component/rui_v3_api/RAKLorawan.*`. The future
Zephyr Arduino Core owns the C++ compatibility facade.

Reserved `.c/.h` pairs live together under their owning `src/` feature
directory and use version `0.0.0`. They are private, contain no callable API,
and do not imply backend support. A public facade is added under `include/rzi/`
only after its contract, backend support, Kconfig, tests, and documentation are
complete.

## Core RUI3 service mapping

- RUI3 `service_lora.c/.h` maps to `include/rzi/lorawan.h`,
  `src/lorawan/lorawan.c`, and the private backend contract.
- ADR, data rate, transmit power, duty cycle, receive-window configuration,
  public-network mode, LBT, join timing, channel masks, and fixed or sub-band
  channel selection remain LoRaWAN Core operations.
- Last-packet RSSI/SNR, protocol version, counters, and session information map
  to the Core information model.
- Class B beacon and ping-slot state remain part of Core class handling.
- DeviceTimeReq and LinkCheckReq map to `src/lorawan/mac_commands/`.
- RUI3 ARSSI maps to `src/lorawan/services/channel_scan/`.
- Multicast session management maps to `src/lorawan/services/multicast/`.
- Certification modes map to `src/lorawan/services/certification/`.
- FUOTA coordination maps to `src/lorawan/services/fuota/`.

Persistent OTAA and ABP setter/getter functions are not duplicated as a second
core API. RZI activation credentials remain explicit inputs to
`rzi_lorawan_join()`. A future provisioning service may own persistent secrets.

## FUOTA package ownership

`src/lorawan/services/fuota/` is the backend-independent coordination boundary
exposed by `include/rzi/fuota.h`. It does not reimplement LoRaWAN application
packages. The USP backend uses the Clock Synchronization, Remote Multicast
Setup, Fragmented Data Block Transport, and Firmware Management Package
implementations built into LoRa Basics Modem. RZI owns event translation,
image access, optional MCUboot installation, and reboot without exposing
Semtech package or fragment-decoder types. See `doc/fuota.md`.

## Raw LoRa and FSK

RUI3 places P2P in `service_lora`, but RZI treats it as a separate service:

```text
src/lora/lora.c
src/lora/lora.h
src/lora/lora_backend.h
src/at/commands/lora/
```

This boundary will own frequency, spreading factor, bandwidth, coding rate,
preamble, power, sync word, CAD, send/receive, encryption, FSK bitrate, and
frequency deviation. Radio coexistence with LoRaWAN must be specified before a
callable API is added.

## AT command-domain mapping

Implemented and reserved LoRaWAN AT files follow the RUI3 command domains:

- `key_id`: OTAA commands are implemented; ABP identifiers and session keys are
  reserved.
- `join_send`: join, confirmation, send, receive, and retry are implemented.
- `network_management`: basic mode, band, and Class A are implemented; ADR,
  duty cycle, data rate, receive windows, transmit power, time request, and LBT
  are reserved.
- `class_b`, `information`, `multicast`, `supplementary`, and `certification`
  have explicit source placeholders.
- Raw LoRa P2P commands have a separate `commands/lora/` package placeholder.

Placeholder files do not register command descriptors. Unsupported commands
continue to return `AT_COMMAND_NOT_FOUND` rather than accepting configuration
that has no effect.

## Backend-only boundaries

RUI3 `LmHandler`, NVM internals, stack test helpers, and direct LoRaMac types are
backend implementation details. RZI does not reserve matching public headers
for them. Backends translate those facilities into RZI-owned types and events.
