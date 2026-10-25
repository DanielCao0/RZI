# RZI samples

Every sample uses only the public RZI API under `include/rzi/`; none of them
includes USP or LoRa Basics Modem headers. Hardware, partitions, and radio
setup come from the RZI product board. A sample carries a board file only
for an application choice, such as the FUOTA console or disabling USB for
the AT port on a board without a USB controller.

Build any sample from the workspace root with sysbuild, then flash
`build/merged_<board>.hex` (see [`doc/boot.md`](../doc/boot.md)):

```sh
west build -p always -b rak4631/nrf52840/rzi --sysbuild rzi/samples/<path>
```

| Sample | What it shows | RAK4631 | RAK3372 | RAK3362 | Needs a LoRaWAN network |
|---|---|---|---|---|---|
| [`lorawan/periodical_uplink`](lorawan/periodical_uplink/README.rst) | OTAA join and periodic uplink; low-power build variant | yes | yes | yes | yes |
| [`lorawan/fuota`](lorawan/fuota/README.rst) | ChirpStack firmware update over LoRaWAN (`rak4631/nrf52840/rzi`, `rak3372_f`) | yes | no | no | yes |
| [`at`](at/README.rst) | RUI3-compatible AT commands (LoRaWAN, P2P, radio tests) over USB CDC or UART | yes | yes | yes | LoRaWAN mode only |
| [`lora/tx_cw`](lora/tx_cw/README.rst) | Continuous-wave carrier for RF measurements | yes | yes | yes | no |
| [`lora/rf_certification`](lora/rf_certification/README.rst) | ETSI, FCC or ARIB certification transmit sequence | yes | yes | yes | no |

Start with `lorawan/periodical_uplink`.

The LoRaWAN samples ship zero-valued credentials in `app.overlay`. Replace
them before a radio test.

Each sample's `README.rst` lists its configuration, the console output to
expect, and what to check when that output differs. Every sample also has a
`sample.yaml`, so CI builds it for each supported board.
