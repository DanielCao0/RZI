# AT command compatibility

Status: implemented

Normative list of AT commands this tree actually registers, and how they
differ from RUI3.

See also: [at-framework.md](./at-framework.md),
[storage-api.md](./storage-api.md),
[rui3-mapping.md](./rui3-mapping.md).

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
  it. The parser returns `AT_ERROR` (`-RZI_ERR_NOT_FOUND` from the registry). There is
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
- `AT_TEST_PARAM_OVERFLOW` (input line longer than `CONFIG_RZI_AT_LINE_MAX`,
  or a handler returned `-RZI_ERR_TOO_LARGE` / `-RZI_ERR_OVERFLOW`)

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
- `AT+BOOT` reboots.
- `AT+FACTORY` restores persisted AT settings and reboots.
- `AT+LOCK` / `AT+PWORD` lock the AT port. While locked, only `PWORD` and
  `LOCK` are accepted.
- `AT+BAUD` reads or sets the UART baud rate when the UART adapter is enabled.
- `AT+ATM` is accepted and stays in AT mode.
- `AT+SLEEP=<ms>` calls `rzi_power_sleep()` when `CONFIG_RZI_POWER=y`.
- `AT+LPM=0` takes the runtime switch's native all-state PM lock;
  `AT+LPM=1` releases that reference. `AT+LPM=?` reads the same runtime
  switch used by the API. PM locks and other native locks do not change
  that readback. Without `CONFIG_PM`, query reports 0 and writes return
  `AT_ERROR`; ordinary CPU idle remains available. Without
  `CONFIG_RZI_POWER`, both reads and writes return `AT_ERROR`.

### Compatible with limits

- `AT+LPMLVL` accepts and reports `1` or `2`, but does not change the power
  policy or select a hardware sleep state.
- `AT+BAT` and `AT+SYSV` are registered and return `AT_ERROR` until a product
  ADC path exists.
- `AT+BLEMAC` returns the Bluetooth identity address when `CONFIG_BT=y`,
  otherwise `AT_ERROR`. Other RUI3 BLE advertising and DTM commands are not
  registered.

## Implemented LoRaWAN commands

### OTAA credentials

These commands are compatible with RUI3 and call `rzi_lorawan_get/set_*`.
Values are hexadecimal, MSB first.

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
  the write returns `AT_ERROR` (`-RZI_ERR_NOT_SUPPORTED`).

### Join

`AT+JOIN`, `AT+JOIN=?`, and
`AT+JOIN=<start>:<auto_join>:<interval>:<attempts>` follow the RUI3 asynchronous
model.

- `start`: `1` starts joining; `0` stops joining, cancels scheduled retries,
  and aborts an attempt still waiting for Join-Accept.
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

The failure event is emitted after all configured retries have failed. On the
USP backend the stack spaces retries with the LoRaWAN regional join duty cycle
and `interval` is stored but not used.
Auto-join settings survive reboot when `CONFIG_RZI_STORAGE=y` and a settings backend
is configured; `CONFIG_RZI_AT_NVM` also enables the storage service.

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

### Network, channel, and last-downlink info

These commands call the public `rzi_lorawan_*` APIs after the LoRaWAN service
has started. A backend that does not implement the operation returns
`AT_ERROR` (`-RZI_ERR_NOT_SUPPORTED`). Timing commands use seconds on the AT wire and
milliseconds in the C API.

- `AT+ADR`, `AT+DR`, `AT+PNM`
- `AT+LBT`, `AT+LBTRSSI`, `AT+LBTSCANTIME`
- `AT+LINKCHECK` (`0` disabled, `1` once, `2` every uplink)
- `AT+TIMEREQ`
- `AT+RSSI`, `AT+SNR`
- `AT+PGSLOT`, `AT+BFREQ`, `AT+BTIME`, `AT+BGW`
- `AT+LTIME` reports GPS seconds (`LTIME: GPS <seconds>`), not RUI3 UTC text
- `AT+ADDMULC`, `AT+RMVMULC`, `AT+LSTMULC` (listed sessions omit keys)
- `AT+CERTIF`
- `AT+DEVADDR`, `AT+NWKSKEY`, `AT+APPSKEY`, `AT+NETID`, `AT+MCROOTKEY`
- `AT+RETY` stores 0 through 7 as shared confirmed-uplink retry policy
- `AT+LPSEND=<port>:<ack>:<hex>` sends one frame up to
  `RZI_LORAWAN_MAX_PAYLOAD`; it is not RUI3 LPTP segmentation

`AT+DR` accepts only the uplink data rates the backend reports for the current
region (LoRa Basics Modem: `smtc_modem_get_enabled_datarates()`).

The following commands stay registered for RUI3 command-set compatibility but
always return `AT_ERROR` (`-RZI_ERR_NOT_SUPPORTED`) on every current backend.
LoRa Basics Modem takes these values from the regional parameters and network
MAC commands and has no application override:

- `AT+TXP`, `AT+DCS`
- `AT+RX1DL`, `AT+RX2DL`, `AT+RX2DR`, `AT+RX2FQ`, `AT+JN1DL`, `AT+JN2DL`

`AT+MASK`, `AT+CHE` and `AT+CHS` follow RUI3 for US915 and AU915 on both
backends. They return `AT_ERROR` for every other band, including CN470. LBM's
CN470 is the RP002 plan, which switches between four sub-plans of 48 or 64
channels, so RUI3's 96-channel numbering does not apply. The commands work
before `AT+JOIN`, are stored, and take effect at the next join. A channel mask
from the network (LinkADRReq) replaces them until the following join.

- `AT+MASK=XXXX`: bit b (0–7) enables 125 kHz channels 8b–8b+7 and 500 kHz
  channel 64+b; bit 8 enables 500 kHz channels 64–71. `0000` enables every
  channel. A read reports `01FF` when every channel is enabled.
- `AT+CHE=n[:n…]`: n = 1–8 enables 125 kHz channels 8(n-1)–8(n-1)+7, n = 9
  enables 64–71, `0` enables every channel. A read lists fully enabled groups,
  or groups with any enabled channel when none is full.
- `AT+CHS=<Hz>`: restricts uplinks to one channel; `0` enables every channel.
  A read returns 0 unless exactly one channel is enabled.

Class B and class C commands (`AT+CLASS=B|C`, `AT+PGSLOT`, `AT+BFREQ`,
`AT+BTIME`, `AT+BGW`) and multicast commands return `AT_ERROR` when the image
does not enable `CONFIG_LORA_BASICS_MODEM_CLASS_B`, `_CLASS_C`, or
`_MULTICAST`. `samples/at` enables all three.

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

- `AT+ARSSI` (channel RSSI scan; not provided)
- P2P long-name aliases (`RFFREQUENCY`, `TXOUTPUTPOWER`, `BANDWIDTH`,
  `SPREADINGFACTOR`, `CODINGRATE`, `PREAMBLELENGTH`) and
  `SYMBOLTIMEOUT` / `FIXLENGTHPAYLOAD`
- `AT+BLEDTM`
- `AT+EXTFLASH`
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

With `CONFIG_RZI_STORAGE=y`, the public LoRaWAN service stores credentials,
activation, band, confirmed uplink mode, retry policy, configured NetID,
channel masks and auto-join settings through Zephyr settings. AT and C APIs
share this storage and restore path. `CONFIG_RZI_AT_NVM` additionally persists
AT-owned system settings and enables RZI_STORAGE. The application must select
and configure a settings backend, normally NVS. `ATR` deletes these values
and restores devicetree defaults after reboot. Legacy AT radio keys migrate
on first read; see [lorawan-api.md](./lorawan-api.md).

Credential read commands intentionally match RUI3 and expose keys in plaintext.
Products that require secret readout protection must enforce it in their
adapter access policy or replace the credential command package. Zephyr
settings/NVS does not encrypt values by itself.
