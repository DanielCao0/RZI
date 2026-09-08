# RZI AT Command Reference and RUI3 Compatibility

## Scope

This document is the normative reference for the AT commands implemented by
RZI. The command grammar, status names, and LoRaWAN events intentionally follow
the [RAK RUI3 AT Command Manual][rui3-manual], but RZI is not a complete RUI3
firmware replacement.

RUI3 serial ports can also enter transparent and protocol modes. RZI
intentionally implements CLI/AT mode only: every received byte passes through
the AT parser, and no adapter forwards arbitrary data to a modem or peripheral.

Compatibility is classified as follows:

- **Compatible**: the RUI3 command forms and observable behavior are supported
  for the implemented LoRaWAN feature set.
- **Compatible with limits**: the grammar is compatible, but RZI or the selected
  backend supports fewer values.
- **Not implemented**: RUI3 defines the command, but RZI does not register it.
  An application receives `AT_COMMAND_NOT_FOUND`.

[rui3-manual]: https://docs.rakwireless.com/product-categories/software-apis-and-libraries/rui3/at-command-manual/

## Command grammar

RZI uses the same operation forms as RUI3:

```text
AT+<COMMAND>?          command description
AT+<COMMAND>=?         read the current value
AT+<COMMAND>=<value>   write a value
AT+<COMMAND>           execute a command
```

Commands are case-insensitive. Values retain their original case. Responses use
CRLF framing and one of the RUI3 status names:

- `OK`
- `AT_ERROR`
- `AT_PARAM_ERROR`
- `AT_BUSY_ERROR`
- `AT_NO_NETWORK_JOINED`
- `AT_COMMAND_NOT_FOUND`

An `OK` response from an asynchronous operation means that the request was
accepted, not that the radio operation has completed.

## Implemented system commands

### Compatible

- `AT` checks communication and returns `OK`.
- `ATZ?` returns help; `ATZ` performs a cold reboot.
- `ATR?` returns help; `ATR` erases RZI AT settings and reboots.
- `AT+VER?` returns help; `AT+VER=?` returns
  `AT+VER=RZI_<version>_<board>`. The value is RZI-specific, as firmware version
  strings are product-specific in RUI3 as well.

## Implemented LoRaWAN commands

### OTAA credentials

These commands are compatible with RUI3. Values are hexadecimal, MSB first.

- `AT+DEVEUI`: read or write the 8-byte DevEUI.
- `AT+APPEUI`: read or write the 8-byte JoinEUI. The `APPEUI` name is retained
  for RUI3 compatibility.
- `AT+APPKEY`: read or write the 16-byte AppKey. RZI supplies this value as both
  the network key and application key when constructing its LoRaWAN 1.0.x/1.1
  neutral OTAA join configuration.

Example:

```text
AT+DEVEUI=1122334455667788
AT+APPEUI=0102030405060708
AT+APPKEY=00112233445566778899AABBCCDDEEFF
```

### Network mode and region

- `AT+NWM` is compatible with limits. `AT+NWM=?` returns `AT+NWM=1`, and
  `AT+NWM=1` succeeds. P2P mode (`0`) is not implemented.
- `AT+BAND` is compatible with limits. RZI accepts RUI3 band numbers 1 through
  11: CN470, RU864, IN865, EU868, US915, AU915, KR920, and AS923 groups 1
  through 4. EU433 (`0`) and LA915 (`12`) are not exposed by the current RZI
  LoRaWAN region API.
- The region must be selected before the LoRaWAN service starts. A later write
  returns `AT_BUSY_ERROR`.

### Activation and class

- `AT+NJM` is compatible with limits. OTAA (`1`) is supported. ABP (`0`) returns
  `AT_PARAM_ERROR`.
- `AT+NJS=?` is compatible and returns `AT+NJS=0` or `AT+NJS=1`.
- `AT+CLASS` is compatible with limits. Class A is supported. Class B and Class
  C return `AT_PARAM_ERROR` until the RZI service and selected backend expose
  those capabilities.

### Join

`AT+JOIN`, `AT+JOIN=?`, and
`AT+JOIN=<start>:<auto_join>:<interval>:<attempts>` follow the RUI3 asynchronous
model.

- `start`: `1` starts joining; `0` stops joining and cancels scheduled retries.
- `auto_join`: `1` starts joining after the next boot; `0` disables it.
- `interval`: 7 through 255 seconds between retries; default is 8.
- `attempts`: 0 through 255 retries after the first attempt; default `0` means
  one attempt in total.

Trailing parameters are optional. Omitted parameters retain their current
values. `AT+JOIN=?` reports the effective settings. A second start while a join
sequence is active returns `AT_BUSY_ERROR`.

The command returns `OK` when the sequence starts. Completion is reported with:

```text
+EVT:JOINED
+EVT:JOIN_FAILED_RX_TIMEOUT
```

The failure event is emitted after all configured retries have failed.
Auto-join settings survive reboot only when `CONFIG_RZI_AT_NVM=y`.

### Uplink, confirmation, and downlink

Confirmation and receive behavior are compatible with RUI3 for Class A:

- `AT+CFM=0|1` selects unconfirmed or confirmed uplinks.
- `AT+CFS=?` reports the result of the last confirmed uplink.
- `AT+RECV=?` consumes and returns the last received application payload as
  `AT+RECV=<port>:<hex_payload>`. A second read returns an empty value unless a
  newer downlink has arrived.

`AT+SEND=<port>:<hex_payload>` is compatible with limits. RZI accepts LoRaWAN
application FPorts 1 through 223 and payloads up to
`RZI_LORAWAN_MAX_PAYLOAD` (currently 242 bytes); the selected region, data rate, and
backend may impose a smaller limit. This is intentionally stricter than the
larger ranges stated in some revisions of the RUI3 manual.

Asynchronous events are:

```text
+EVT:TX_DONE
+EVT:SEND_CONFIRMED_OK
+EVT:SEND_CONFIRMED_FAILED
+EVT:RX_1:<rssi>:<snr>:UNICAST:<port>:<payload>
```

## RUI3 commands not implemented

RZI deliberately does not register commands whose service semantics are not
implemented. This avoids accepting configuration that the backend silently
ignores.

The main gaps compared with the full RUI3 command set are:

- ABP credentials and activation, including `AT+DEVADDR`, `AT+APPSKEY`, and
  `AT+NWKSKEY`;
- radio policy and channel configuration, including ADR, data rate, transmit
  power, channel mask, duty-cycle, and link-check commands;
- Class B, Class C, multicast, and LoRaWAN package commands;
- LoRa P2P commands;
- RUI3 module-specific hardware, BLE, system, filesystem, and firmware-update
  commands.

Therefore, software using only the implemented OTAA/Class A subset can use the
same command flow on RUI3 and RZI. Software depending on the wider RUI3 command
catalog requires feature detection or an RZI-specific adaptation.

## Persistence and security

With `CONFIG_RZI_AT_NVM=y`, RZI stores DevEUI, JoinEUI, AppKey, band, confirmed
uplink mode, and auto-join settings through Zephyr settings. The application
must select and configure a settings backend, normally NVS. `ATR` deletes these
values and restores devicetree defaults after reboot.

Credential read commands intentionally match RUI3 and expose keys in plaintext.
Products that require secret readout protection must enforce it in their
adapter access policy or replace the credential command package. Zephyr
settings/NVS does not encrypt values by itself.
