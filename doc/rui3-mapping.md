# RUI3 mapping

Status: implemented

Public C surface that corresponds to the RUI3 C service
(`service_lora*`). Headers are the ABI. This page is the function index,
backend coverage, and C-level gap versus RUI3. AT grammar and events are
in [at-command-compatibility.md](./at-command-compatibility.md).

See also: [lorawan-api.md](./lorawan-api.md),
[at-command-compatibility.md](./at-command-compatibility.md),
[lora-api.md](./lora-api.md).

## Purpose

These signatures are the RZI LoRaWAN ABI. Unsupported backends return
`-RZI_ERR_NOT_SUPPORTED`. The reference behavior is the RUI3 C service and C AT
implementation, not `component/rui_v3_api/RAKLorawan.*`. Persistent
OTAA/ABP credentials stay inputs to `rzi_lorawan_join()`. Join retry,
confirm-default, and last-payload readback stay in the AT package.
P2P/FSK is `include/rzi/lora/lora.h`, not this ABI.

`tx_power` is the LoRaWAN power index, not dBm. `sub_band` is 0 (all) or
1–8. `query_tx_possible()` returns `0` when `size` fits the current data
rate. Class B ping-slot periodicity is 0–7. Channel masks use up to
`RZI_LORAWAN_CHANNEL_MASK_WORDS` (6) words.

## `include/rzi/lorawan/lorawan.h`

Core lifecycle, network, channel, last-downlink RSSI/SNR, and Class B.
`rzi_lorawan_callbacks.on_event` reports all public asynchronous results,
including LinkCheck, DeviceTime, and Class B state changes.

```c
int rzi_lorawan_register_callbacks(const struct rzi_lorawan_callbacks *callbacks,
                                   rzi_lorawan_callback_handle_t *handle);
int rzi_lorawan_unregister_callbacks(rzi_lorawan_callback_handle_t handle);
int rzi_lorawan_set_region(enum rzi_lorawan_region region);
int rzi_lorawan_set_join_backoff_bypass(bool enabled);
int rzi_lorawan_start(void);
int rzi_lorawan_join(const struct rzi_lorawan_join_config *config);
int rzi_lorawan_leave(void);
int rzi_lorawan_send(uint8_t port, const uint8_t *data, size_t size,
                     enum rzi_lorawan_message_type type);
int rzi_lorawan_set_class(enum rzi_lorawan_class device_class);
int rzi_lorawan_is_joined(bool *joined);
uint32_t rzi_lorawan_get_capabilities(void);

int rzi_lorawan_get_region(enum rzi_lorawan_region *region);
int rzi_lorawan_get_class(enum rzi_lorawan_class *device_class);
int rzi_lorawan_get_adr(bool *enabled);
int rzi_lorawan_set_adr(bool enabled);
int rzi_lorawan_get_data_rate(enum rzi_lorawan_data_rate *data_rate);
int rzi_lorawan_set_data_rate(enum rzi_lorawan_data_rate data_rate);
int rzi_lorawan_get_tx_power(uint8_t *tx_power);
int rzi_lorawan_set_tx_power(uint8_t tx_power);
int rzi_lorawan_get_duty_cycle(bool *enabled);
int rzi_lorawan_set_duty_cycle(bool enabled);
int rzi_lorawan_get_rx1_delay(uint32_t *delay_ms);
int rzi_lorawan_set_rx1_delay(uint32_t delay_ms);
int rzi_lorawan_get_rx2_delay(uint32_t *delay_ms);
int rzi_lorawan_set_rx2_delay(uint32_t delay_ms);
int rzi_lorawan_get_rx2_data_rate(enum rzi_lorawan_data_rate *data_rate);
int rzi_lorawan_set_rx2_data_rate(enum rzi_lorawan_data_rate data_rate);
int rzi_lorawan_get_rx2_frequency(uint32_t *frequency_hz);
int rzi_lorawan_set_rx2_frequency(uint32_t frequency_hz);
int rzi_lorawan_get_join_accept_delay1(uint32_t *delay_ms);
int rzi_lorawan_set_join_accept_delay1(uint32_t delay_ms);
int rzi_lorawan_get_join_accept_delay2(uint32_t *delay_ms);
int rzi_lorawan_set_join_accept_delay2(uint32_t delay_ms);
int rzi_lorawan_get_public_network(bool *enabled);
int rzi_lorawan_set_public_network(bool enabled);
int rzi_lorawan_get_lbt(bool *enabled);
int rzi_lorawan_set_lbt(bool enabled);
int rzi_lorawan_get_lbt_rssi(int16_t *rssi_dbm);
int rzi_lorawan_set_lbt_rssi(int16_t rssi_dbm);
int rzi_lorawan_get_lbt_scan_time(uint32_t *time_ms);
int rzi_lorawan_set_lbt_scan_time(uint32_t time_ms);

int rzi_lorawan_get_channel_mask(uint16_t *mask, size_t words);
int rzi_lorawan_set_channel_mask(const uint16_t *mask, size_t words);
int rzi_lorawan_get_sub_band(uint8_t *sub_band);
int rzi_lorawan_set_sub_band(uint8_t sub_band);
int rzi_lorawan_get_fixed_channel(uint32_t *frequency_hz);
int rzi_lorawan_set_fixed_channel(uint32_t frequency_hz);

int rzi_lorawan_get_last_rssi(int16_t *rssi_dbm);
int rzi_lorawan_get_last_snr(int8_t *snr_quarter_db);
int rzi_lorawan_get_protocol_version(const char **version);
int rzi_lorawan_get_net_id(uint32_t *net_id);
int rzi_lorawan_get_dev_nonce(uint16_t *dev_nonce);
int rzi_lorawan_set_dev_nonce(uint16_t dev_nonce);
int rzi_lorawan_query_tx_possible(size_t size);
int rzi_lorawan_is_busy(bool *busy);

int rzi_lorawan_get_ping_slot_periodicity(uint8_t *periodicity);
int rzi_lorawan_set_ping_slot_periodicity(uint8_t periodicity);
int rzi_lorawan_get_beacon_frequency(uint32_t *frequency_hz);
int rzi_lorawan_get_beacon_time(uint32_t *gps_time);
int rzi_lorawan_get_beacon_data_rate(enum rzi_lorawan_data_rate *data_rate);
int rzi_lorawan_get_beacon_gateway(struct rzi_lorawan_beacon_gateway *gateway);
int rzi_lorawan_get_class_b_state(enum rzi_lorawan_class_b_state *state);
int rzi_lorawan_stop_class_b(void);
```

`get_last_rssi` / `get_last_snr` are filled from the last application
downlink. `get_protocol_version` is `"LoRaWAN 1.0.4"`.

## LinkCheck and DeviceTime (`lorawan.h`)

```c
int rzi_lorawan_get_link_check_mode(enum rzi_lorawan_link_check_mode *mode);
int rzi_lorawan_request_link_check(enum rzi_lorawan_link_check_mode mode);
int rzi_lorawan_get_device_time_enabled(bool *enabled);
int rzi_lorawan_request_device_time(bool enabled);
int rzi_lorawan_get_network_time(struct rzi_lorawan_network_time *time);
```

`request_link_check(ONCE)` or `EVERY_UPLINK` triggers a request immediately.
Completion is `on_event()` with `RZI_LORAWAN_EVENT_LINK_CHECK` or
`RZI_LORAWAN_EVENT_DEVICE_TIME`.
`get_network_time` uses the GPS epoch.

## `include/rzi/lorawan/multicast.h`

```c
int rzi_lorawan_add_multicast_session(const struct rzi_lorawan_multicast_session *session);
int rzi_lorawan_remove_multicast_session(uint32_t dev_addr);
int rzi_lorawan_get_multicast_count(size_t *count);
int rzi_lorawan_get_multicast_session(size_t index,
                                      struct rzi_lorawan_multicast_session *session);
int rzi_lorawan_clear_multicast_sessions(void);
```

Groups are 0–3. `group_id` of `-1` allocates the next free slot. Listed
sessions omit keys.

## `include/rzi/lorawan/certification.h`

```c
int rzi_lorawan_get_certification_mode(bool *enabled);
int rzi_lorawan_set_certification_mode(bool enabled);
int rzi_lorawan_get_certification_port_enabled(bool *enabled);
int rzi_lorawan_set_certification_port_enabled(bool enabled);
```

## `include/rzi/lorawan/fuota.h`

FUOTA coordination stays Kconfig-gated (`CONFIG_RZI_LORAWAN_FUOTA`). See
[fuota.md](./fuota.md).

## Backend coverage

Public facades always compile except FUOTA. A capability bit means the
backend implements that group. A NULL operation still returns `-RZI_ERR_NOT_SUPPORTED`.

| Group | USP / LBM | Zephyr `lorawan_*` |
|---|---|---|
| Join / send / class | OTAA, Class A/B/C | OTAA, ABP, Class A/C |
| ADR, DR, public network, LBT | Yes | ADR, DR |
| TX power, duty-cycle enable, RX / join windows | `-RZI_ERR_NOT_SUPPORTED` (not in `smtc_modem_*`) | `-RZI_ERR_NOT_SUPPORTED` |
| Channel mask / sub-band / fixed channel | `-RZI_ERR_NOT_SUPPORTED` | Channel mask |
| RSSI / SNR / protocol version | Core downlink cache / constant | Same |
| `query_tx_possible` / `is_busy` / DevNonce | TX size and busy | TX size, busy, DevNonce |
| Class B ping-slot / state | Yes; beacon fields `-RZI_ERR_NOT_SUPPORTED` | `-RZI_ERR_NOT_SUPPORTED` |
| LinkCheckReq / DeviceTimeReq | Yes | Yes (not under `CONFIG_LORAWAN_EMUL`) |
| Multicast 0–3 | Yes | `-RZI_ERR_NOT_SUPPORTED` |
| Certification | Mode yes; FPort is local | `-RZI_ERR_NOT_SUPPORTED` |
| FUOTA | When `CONFIG_RZI_LORAWAN_FUOTA` | When enabled |

AT commands wrap these APIs. `-RZI_ERR_NOT_SUPPORTED` becomes `AT_ERROR`.

## Out of this ABI

Do not add these as `rzi_lorawan_*`:

- credential get/set (`dev_eui`, `join_eui`, `app_key`, ABP keys) — `join()`
- confirm default, last confirm status, send retry — AT `CFM` / `CFS` / `RETY`
- auto-join period and attempt count — AT `JOIN`
- last received payload buffer — AT `RECV`; C uses `downlink`
- work-mode switch to P2P/FSK — `rzi_lora_*`
- sleep / suspend / resume — `rzi_power_*`
- LPTP, LmHandler, LoRaMac types
- channel RSSI scan (`ARSSI`)

## RUI3 C coverage

RUI3 source of truth: `component/service/lora/service_lora*.h`.
Out of product scope for RAK4631 / RAK3372: cellular, bootloader-only
commands, RAK3172 `AT+UID`, and transparent / binary API modes.

| Status | Meaning |
|---|---|
| present | Same capability exists |
| limited | Exists, with a documented subset or different model |
| AT-only | RUI3 has a C getter/setter; RZI keeps it in AT / `join()` |
| missing | In RUI3 product firmware, not in RZI |
| out of scope | Other SKU or bootloader |

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
| Info | RSSI, SNR, version | matching getters | present | SNR is quarter-dB in C, AT prints dB |
| Info | ARSSI | — | missing | Channel RSSI scan not provided |
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

C API still missing if the goal is parity with `service_lora*`:

- LPTP (`service_lora_lptp_send`)
- `region_isActive`, `get_real_class_from_stack`, `systemMaxRxError`
- channel RSSI scan (`ARSSI`)
- P2P AES encrypt/decrypt helpers, CAD-send callback, `get_radio_stat`,
  standalone `check_runtime_*`
- Radio-test hopping (`TCONF` hop fields, `TTH`/`TRTH` as RUI implements them)

Intentional: credentials / CFM / RETY / auto-join / last payload stay
AT-only; `LPSEND` is not LPTP; P2P encrypt is XOR; EU433 / LA915 are not
regions.
