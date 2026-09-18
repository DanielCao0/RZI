# RUI3 vs RZI gap

Status: implemented

Side-by-side coverage of the RUI3 C service (`service_lora*`) and the AT
commands registered on RAK4631 / RAK3372, versus this tree.

See also: [rui3-mapping.md](./rui3-mapping.md),
[at-command-compatibility.md](./at-command-compatibility.md),
[lora-api.md](./lora-api.md),
[lorawan-api.md](./lorawan-api.md).

## Scope

RUI3 source of truth:

- C API: `component/service/lora/service_lora*.h`
- AT names: `component/service/mode/cli/atcmd_*_def.h` as registered in
  `atcmd.c` for nRF52840 / STM32WL LoRaWAN + P2P firmware

Out of product scope for RAK4631 / RAK3372: cellular (`ATCELL`),
bootloader-only (`AT+BOOTSTATUS`, `AT+RUN`, `AT+UPDATE`), RAK3172 `AT+UID`,
RAK11160 ESP commands, transparent / binary API modes (`AT+APM`, `ATD`).

Status values:

| Status | Meaning |
|---|---|
| present | Same capability exists |
| limited | Exists, with a documented subset or different model |
| AT-only | RUI3 has a C getter/setter; RZI keeps it in AT / `join()` on purpose |
| missing | In RUI3 product firmware, not in RZI |
| out of scope | Other SKU or bootloader |
| RUI unused | RUI3 defines a macro but does not register the command |

## API table

RUI3 `service_lora_*` versus public `rzi_lorawan_*` / `rzi_lora_*` /
`rzi_power_*` / `rzi_fuota_*`. Credential get/set, confirm-default, retry,
auto-join, last payload, and network-work-mode are AT-only by ABI policy.

| Group | RUI3 | RZI | Status | Note |
|---|---|---|---|---|
| Lifecycle | `service_lora_init` | `rzi_lorawan_start` | present | |
| Lifecycle | `service_lora_get/set_band` | `rzi_lorawan_get/set_region` | limited | No EU433 / LA915 enum |
| Lifecycle | `service_lora_region_isActive` | — | missing | No public “region compiled in?” probe |
| Lifecycle | `service_lora_set_lora_default` | `ATR` / `AT+FACTORY` | AT-only | Not a C ABI |
| Credentials | `get/set` AppEUI, AppKey, DevEUI, DevAddr, NwkSKey, AppSKey | `rzi_lorawan_join()` inputs; AT keys | AT-only | Stay in `join()` / AT |
| Credentials | `get/set_nwk_id` | `rzi_lorawan_get_net_id`; AT `NETID` | AT-only | Set stays in AT |
| Credentials | `get_McRoot_key` | AT `MCROOTKEY` | AT-only | |
| Join / send | `service_lora_join` | `rzi_lorawan_join` | present | Auto-join period/count stay AT `JOIN=` |
| Join / send | `get/set_njm`, `get_njs` | `join()` mode; `rzi_lorawan_is_joined` | present | NJM also AT |
| Join / send | `get/set_nwm` | `rzi_lora_start/stop`; AT `NWM` | AT-only | P2P vs LoRaWAN is not LoRaWAN ABI |
| Join / send | `get/set_retry`, `get/set_cfm`, `get_cfs` | AT `RETY` / `CFM` / `CFS` | AT-only | |
| Join / send | `get/set_join_start`, `auto_join*` | AT `JOIN=` fields | AT-only | |
| Join / send | `service_lora_send` | `rzi_lorawan_send` | present | Confirm flag comes from AT context |
| Join / send | `get_last_recv` | AT `RECV` | AT-only | |
| Join / send | `service_lora_lptp_send` | AT `LPSEND` | missing | LPSEND is one frame, max 242, not LPTP |
| Network | ADR, class, DCS, DR, TXP, PNM | matching `rzi_lorawan_get/set_*` | present | `tx_power` is LoRaWAN index |
| Network | `get_real_class_from_stack` | `rzi_lorawan_get_class` | missing | No separate stack-vs-requested class |
| Network | RX1/RX2 delays, RX2 DR/freq, JN1/JN2 | matching getters/setters | present | |
| Network | LBT / RSSI / scantime | matching getters/setters | present | |
| Network | `get/set_linkcheck` | `rzi_lorawan_request_link_check` | present | |
| Network | `get/set_timereq` | `rzi_lorawan_request_device_time` | present | |
| Network | `query_txPossible`, `isbusy` | `query_tx_possible`, `is_busy` | present | `0` = request accepted |
| Network | `get/set_DevNonce` | `get/set_dev_nonce` | present | |
| Network | `systemMaxRxError` | — | missing | Internal RX-window calibration |
| Channels | mask, CHE, CHS | `channel_mask`, `sub_band`, `fixed_channel` | present | |
| Info | RSSI, SNR, ARSSI, version | matching getters | present | SNR is quarter-dB in C, AT prints dB |
| Class B | ping slot, beacon freq/time/DR, BGW, state, force stop | matching getters/setters | present | |
| Class B | `get_local_time` UTC text | `get_network_time` / AT `LTIME` | limited | GPS seconds, not RUI UTC string |
| MAC | recv/join/send/linkcheck/timereq callbacks | `rzi_lorawan_register_callbacks` | present | |
| Cert | `certification`, `IsCertPortOn` | `set_certification_mode`, `set_certification_port_enabled` | present | |
| Multicast | add / remove / list / clear | `rzi_lorawan_*_multicast_session` | present | |
| Power | `suspend` / `resume` | `rzi_power_sleep` / `set_policy` | present | Different names |
| FUOTA | RUI LmHandler FUOTA packages | `rzi_fuota_*` | limited | ChirpStack dual-slot, not LoRa Alliance FUOTA |
| P2P | `p2p_init/config/send/recv` | `rzi_lora_start/set_config/send/receive` | present | USP cannot share radio with LoRaWAN |
| P2P | freq/SF/BW/CR/preamble/power/IQ/sync/CAD/FSK | `rzi_lora_get/set_config` | present | |
| P2P | `check_runtime_*` validators | inside `set_config` | missing | No standalone validator APIs |
| P2P | crypto enable/key/IV | `config.encrypt/key/iv` | limited | XOR of key+IV, not AES-CTR |
| P2P | `p2p_encrpty` / `decrpty` | — | missing | No public AES helpers |
| P2P | send / recv callbacks | `rzi_lora_register_callbacks` | present | |
| P2P | `register_send_CAD_cb` | — | missing | No dedicated CAD-send callback |
| P2P | `get/set_public_network` | `config.sync_word` | limited | No separate P2P public-network flag |
| P2P | `get/set_symbol_timeout` | `config.symbol_timeout` | present | No AT command |
| P2P | `get/set_fix_length_payload` | `config.fixed_length` | present | No AT command |
| P2P | `get_radio_stat` | — | missing | No “radio busy” P2P getter |
| Test | TRSSI, TTONE, TTX, TRX, TOFF, CW | `rzi_lora_test_*` | present | |
| Test | `get/set_tconf` | `get/set_config` | limited | Six P2P fields, not RUI long TCONF |
| Test | `tth` | `test_tx` | limited | No frequency hopping |
| Test | `trth` | `test_rx` | limited | RUI is random TX hop; RZI is RX |
| Test | `set_dr/txp_for_trth` | — | missing | |

## AT table

Commands RUI3 registers on RAK4631 / RAK3372 application firmware.

| Group | RUI3 | RZI | Status | Note |
|---|---|---|---|---|
| Core | `AT` | `AT` | present | |
| Core | `ATZ` | `ATZ` | present | |
| Core | `ATR` | `ATR` | present | |
| Core | `ATE` | `ATE` | present | |
| General | `AT+BOOT` | `BOOT` | present | |
| General | `AT+BOOTVER` | `BOOTVER` | limited | Fixed `RZI MCUboot` string |
| General | `AT+DEBUG` | `DEBUG` | present | |
| General | `AT+FSN` | `FSN` | present | |
| General | `AT+FACTORY` | `FACTORY` | present | |
| General | `AT+SN` | `SN` | present | |
| General | `AT+BAT` | `BAT` | limited | Returns `AT_ERROR` (`-ENOTSUP`) |
| General | `AT+SYSV` | `SYSV` | limited | Returns `AT_ERROR` (`-ENOTSUP`) |
| General | `AT+BUILDTIME` | `BUILDTIME` | present | |
| General | `AT+REPOINFO` | `REPOINFO` | limited | Version stub, not git SHAs |
| General | `AT+VER` | `VER` | present | |
| General | `AT+FIRMWAREVER` | `FIRMWAREVER` | present | |
| General | `AT+CLIVER` | `CLIVER` | present | |
| General | `AT+APIVER` | `APIVER` | present | |
| General | `AT+HWMODEL` | `HWMODEL` | present | `CONFIG_BOARD` |
| General | `AT+HWID` | `HWID` | present | `CONFIG_SOC` |
| General | `AT+ALIAS` | `ALIAS` | present | |
| General | `AT+BLEMAC` | `BLEMAC` | limited | Needs `CONFIG_BT`; read-only |
| General | `AT+BLEDTM` | — | missing | RAK4631 RUI registers it |
| General | `AT+UID` | — | out of scope | RAK3172 only |
| Sleep | `AT+SLEEP` | `SLEEP` | present | `rzi_power_sleep` |
| Sleep | `AT+LPM` | `LPM` | present | |
| Sleep | `AT+LPMLVL` | `LPMLVL` | present | |
| Serial | `AT+LOCK` | `LOCK` | present | |
| Serial | `AT+PWORD` | `PWORD` | present | |
| Serial | `AT+BAUD` | `BAUD` | present | |
| Serial | `AT+ATM` | `ATM` | present | Already in AT mode |
| Serial | `AT+APM` | — | out of scope | Binary API mode |
| Serial | `ATD` / `+++` | — | out of scope | Transparent mode |
| Keys | `DEVEUI` `APPEUI` `APPKEY` | same | present | |
| Keys | `DEVADDR` `NWKSKEY` `APPSKEY` | same | present | Fed to `join()` for ABP |
| Keys | `NETID` | `NETID` | present | |
| Keys | `MCROOTKEY` | `MCROOTKEY` | present | Read |
| Join | `NJM` `NJS` `JOIN` `SEND` `RECV` | same | present | |
| Join | `CFM` `CFS` `RETY` | same | present | AT-only state |
| Join | `LPSEND` | `LPSEND` | limited | Single frame, max 242; not LPTP |
| Join | `USEND` | — | RUI unused | Commented out in `atcmd.c` |
| Network | `NWM` | `NWM` | present | Production reboot skipped under ZTEST |
| Network | `BAND` | `BAND` | limited | No EU433 / LA915 |
| Network | `CLASS` `ADR` `DCS` `DR` `TXP` `PNM` | same | present | |
| Network | `RX1DL` `RX2DL` `RX2DR` `RX2FQ` `JN1DL` `JN2DL` | same | present | |
| Network | `LINKCHECK` `TIMEREQ` | same | present | |
| Network | `LBT` `LBTRSSI` `LBTSCANTIME` | same | present | |
| Class B | `PGSLOT` `BFREQ` `BTIME` `BGW` | same | present | |
| Class B | `LTIME` | `LTIME` | limited | GPS time, not RUI UTC text |
| Info | `RSSI` `SNR` `ARSSI` | same | present | |
| Channel | `MASK` `CHE` `CHS` | same | present | |
| Multicast | `ADDMULC` `RMVMULC` `LSTMULC` | same | present | |
| Cert | `CERTIF` | `CERTIF` | present | |
| RF test | `TRSSI` `TTONE` `TTX` `TRX` `TOFF` `CW` | same | present | |
| RF test | `TCONF` | `TCONF` | limited | `freq:sf:bw:cr:pl:pwr`, not RUI long form |
| RF test | `TTH` | `TTH` | limited | No hop sequence |
| RF test | `TRTH` | `TRTH` | limited | RX test, not random TX hop |
| P2P | `P2P` `PFREQ` `PSF` `PBW` `PCR` `PPL` `PTP` | same | present | |
| P2P | `PSEND` `PRECV` `CAD` | same | present | |
| P2P | `ENCRY` `ENCKEY` `CRYPIV` | same | limited | XOR, not AES |
| P2P | `IQINVER` `SYNCWORD` `PBR` `PFDEV` | same | present | |
| P2P | `RFFREQUENCY` `TXOUTPUTPOWER` `BANDWIDTH` | — | missing | Aliases of `PFREQ` / `PTP` / `PBW` |
| P2P | `SPREADINGFACTOR` `CODINGRATE` `PREAMBLELENGTH` | — | missing | Aliases of `PSF` / `PCR` / `PPL` |
| P2P | `SYMBOLTIMEOUT` | — | missing | Field exists on `rzi_lora_config` |
| P2P | `FIXLENGTHPAYLOAD` | — | missing | Field exists on `rzi_lora_config` |
| Flash | `AT+EXTFLASH` | — | missing | RUI `SUPPORT_EXTFLASH` |
| Cellular | `ATCELL` | — | out of scope | RAK5010 |
| Bootloader | `AT+BOOTSTATUS` `AT+RUN` `AT+UPDATE` | — | out of scope | Bootloader firmware |

## Product-scope gaps

C API still missing if the goal is parity with `service_lora*`:

- LPTP (`service_lora_lptp_send`)
- `region_isActive`, `get_real_class_from_stack`, `systemMaxRxError`
- P2P AES encrypt/decrypt helpers, CAD-send callback, `get_radio_stat`,
  standalone `check_runtime_*`
- Radio-test hopping (`TCONF` hop fields, `TTH`/`TRTH` as RUI implements them)

AT still missing on RAK4631 / RAK3372 application firmware:

- P2P long-name aliases and `SYMBOLTIMEOUT` / `FIXLENGTHPAYLOAD`
- `AT+BLEDTM`
- `AT+EXTFLASH`

Intentional: credentials / CFM / RETY / auto-join / last payload stay AT-only;
`LPSEND` is not LPTP; P2P encrypt is XOR; EU433 / LA915 are not regions;
cellular, bootloader, and transparent/API modes stay out of this tree.
