# AT command compatibility

Status: implemented

Normative list of AT commands this tree actually registers, and how they
differ from RUI3.

See also: [at-framework.md](./at-framework.md),
[storage-api.md](./storage-api.md),
[rui3-gap.md](./rui3-gap.md).

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
- **Not implemented**: RUI3 defines the command, but RZI does not register
  it. The parser returns `AT_ERROR` (`-ENOENT` from the registry). There is
  no `AT_COMMAND_NOT_FOUND` status in this tree.

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
- `AT_TEST_PARAM_OVERFLOW` (input line longer than `CONFIG_RZI_AT_LINE_MAX`)

An `OK` response from an asynchronous operation means that the request was
accepted, not that the radio operation has completed.

## Implemented system commands

### Compatible

- `AT` checks communication and returns `OK`.
- `AT?` lists every registered command and its help text.
- `ATE` toggles input echo; `ATE?` returns help.
- `ATZ?` returns help; `ATZ` performs a cold reboot.
- `ATR?` returns help; `ATR` erases RZI AT settings and reboots.
- `AT+VER?` returns help; `AT+VER=?` returns
  `AT+VER=RZI_<version>_<board>`. The value is RZI-specific, as firmware version
  strings are product-specific in RUI3 as well.
- `AT+FIRMWAREVER`, `AT+CLIVER`, `AT+APIVER` report `RZI_VERSION_STRING`.
- `AT+HWMODEL` reports `CONFIG_BOARD`; `AT+HWID` reports `CONFIG_SOC`.
- `AT+SN` and `AT+FSN` report a serial derived from `hwinfo` when available.
- `AT+ALIAS` stores an operator name (max 16 characters).
- `AT+BUILDTIME`, `AT+REPOINFO`, `AT+BOOTVER`, `AT+DEBUG`.
- `AT+BOOT` reboots; with RSUP it arms the bootloader slot.
- `AT+FACTORY` restores persisted AT settings and reboots.
- `AT+LOCK` / `AT+PWORD` lock the AT port. While locked, only `PWORD` and
  `LOCK` are accepted.
- `AT+BAUD` reads or sets the UART baud rate when the UART adapter is enabled.
- `AT+ATM` is accepted and stays in AT mode.
- `AT+SLEEP=<ms>` calls `rzi_power_sleep()` when `CONFIG_RZI_POWER=y`.
- `AT+LPM` / `AT+LPMLVL` select the power policy when the power service is
  compiled in.

### Compatible with limits

- `AT+BAT` and `AT+SYSV` are registered and return `AT_ERROR` until a product
  ADC path exists.
- `AT+BLEMAC` returns the Bluetooth identity address when `CONFIG_BT=y`,
  otherwise `AT_ERROR`. Other RUI3 BLE advertising and DTM commands are not
  registered.

## Implemented LoRaWAN commands

### OTAA credentials

These commands are compatible with RUI3. Values are hexadecimal, MSB first.

- `AT+DEVEUI`: read or write the 8-byte DevEUI.
- `AT+APPEUI`: read or write the 8-byte JoinEUI. The `APPEUI` name is retained
  for RUI3 compatibility.
- `AT+APPKEY`: read or write the 16-byte AppKey. Join copies that value into
  both `network_key` (1.0.x AppKey) and `application_key` (also AppKey, so
  there is no separate GenAppKey on the AT path).

Example:

```text
AT+DEVEUI=1122334455667788
AT+APPEUI=0102030405060708
AT+APPKEY=00112233445566778899AABBCCDDEEFF
```

### Network mode and region

- `AT+NWM` is compatible with limits. `0` = P2P LoRa, `1` = LoRaWAN, `2` =
  P2P FSK. P2P values require `CONFIG_RZI_AT_COMMAND_LORA=y`; otherwise the
  write returns `AT_ERROR`. Changing the mode persists and reboots on
  production images.
- `AT+BAND` is compatible with limits. RZI accepts RUI3 band numbers 1 through
  11: CN470, RU864, IN865, EU868, US915, AU915, KR920, and AS923 groups 1
  through 4. EU433 (`0`) and LA915 (`12`) are not exposed by the current RZI
  LoRaWAN region API.
- The region must be selected before the LoRaWAN service starts. A later write
  returns `AT_BUSY_ERROR`.

### Activation and class

- `AT+NJM` is compatible with limits. OTAA (`1`) is supported. ABP (`0`) is
  accepted when the backend advertises `RZI_LORAWAN_CAP_ABP`; otherwise the
  write returns `AT_ERROR`.
- `AT+NJS=?` is compatible and returns `AT+NJS=0` or `AT+NJS=1`.
- `AT+CLASS` is compatible with limits. `A`, `B`, and `C` are accepted when
  the selected backend advertises the matching class capability; otherwise
  the write returns `AT_ERROR` (`-ENOTSUP`).

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

### Network, channel, and information

These commands call the public `rzi_lorawan_*` APIs after the LoRaWAN service
has started. A backend that does not implement the operation returns
`AT_ERROR` (`-ENOTSUP`). Timing commands use seconds on the AT wire and
milliseconds in the C API.

- `AT+ADR`, `AT+DR`, `AT+TXP`, `AT+DCS`, `AT+PNM`
- `AT+RX1DL`, `AT+RX2DL`, `AT+RX2DR`, `AT+RX2FQ`, `AT+JN1DL`, `AT+JN2DL`
- `AT+LBT`, `AT+LBTRSSI`, `AT+LBTSCANTIME`
- `AT+LINKCHECK` (`0` disabled, `1` once, `2` every uplink)
- `AT+TIMEREQ`
- `AT+MASK`, `AT+CHE`, `AT+CHS`
- `AT+RSSI`, `AT+SNR`, `AT+ARSSI`
- `AT+PGSLOT`, `AT+BFREQ`, `AT+BTIME`, `AT+BGW`
- `AT+LTIME` reports GPS seconds (`LTIME: GPS <seconds>`), not RUI3 UTC text
- `AT+ADDMULC`, `AT+RMVMULC`, `AT+LSTMULC` (listed sessions omit keys)
- `AT+CERTIF`
- `AT+DEVADDR`, `AT+NWKSKEY`, `AT+APPSKEY`, `AT+NETID`, `AT+MCROOTKEY`
- `AT+RETY` stores 0 through 7 confirmed-uplink retries for the AT package
- `AT+LPSEND=<port>:<ack>:<hex>` sends one frame up to
  `RZI_LORAWAN_MAX_PAYLOAD`; it is not RUI3 LPTP segmentation

## Implemented P2P and radio-test commands

These commands require `CONFIG_RZI_AT_COMMAND_LORA=y` and call `rzi_lora_*`.
They return `AT_BUSY_ERROR` while `AT+NWM=1`.

### Compatible with limits

- `AT+P2P`, `AT+PFREQ`, `AT+PSF`, `AT+PBW`, `AT+PCR`, `AT+PPL`, `AT+PTP`
- `AT+PSEND=<hex>`, `AT+PRECV=<timeout_ms>`
- `AT+CAD`, `AT+ENCRY`, `AT+ENCKEY`, `AT+CRYPIV`, `AT+IQINVER`, `AT+SYNCWORD`
- `AT+PBR`, `AT+PFDEV`
- `AT+TRSSI`, `AT+TTONE`, `AT+TTX`, `AT+TRX`, `AT+TCONF`, `AT+TTH`, `AT+TRTH`,
  `AT+TOFF`, `AT+CW`

P2P events are `+EVT:TXP2P DONE` and `+EVT:RXP2P:<rssi>:<snr>:<hex>`.
Encryption XORs the payload with the stored key and IV. `AT+TCONF` uses the
same six-field form as `AT+P2P`, not the longer RUI3 test-config list.
`AT+TTH` / `AT+TRTH` accept `start:stop:hop:count` and transmit or receive
`count` packets without changing frequency.

## RUI3 commands not implemented

RZI deliberately does not register commands whose service semantics are not
implemented. This avoids accepting configuration that the backend silently
ignores.

The main gaps compared with the full RUI3 command set are:

- Cellular, GNSS, and WisBlock-sensor commands;
- BLE advertising, UART-over-BLE, and DTM commands other than `AT+BLEMAC`;
- Bootloader-only `AT+RUN`, `AT+BOOTSTATUS`, and `AT+UPDATE`;
- Transparent / protocol modes (`AT+APM`, `ATD`, `+++`);
- RUI3 LPTP segmentation (`AT+LPSEND` here is a single frame);
- Filesystem and external-flash management commands.

Cellular, bootloader-only, and passthrough commands stay out of scope on
RAK4631 / RAK3372 product images. Software using the documented LoRaWAN, P2P,
system, and radio-test commands can share command names with RUI3. Values that
the selected backend does not support return `AT_ERROR`.

## Persistence and security

With `CONFIG_RZI_AT_NVM=y`, RZI stores DevEUI, JoinEUI, AppKey, band, confirmed
uplink mode, and auto-join settings through Zephyr settings. The application
must select and configure a settings backend, normally NVS. `ATR` deletes these
values and restores devicetree defaults after reboot.

Credential read commands intentionally match RUI3 and expose keys in plaintext.
Products that require secret readout protection must enforce it in their
adapter access policy or replace the credential command package. Zephyr
settings/NVS does not encrypt values by itself.
