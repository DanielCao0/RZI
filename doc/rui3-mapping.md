# RUI3 mapping

Status: implemented

Public `rzi_lorawan_*` surface that corresponds to the RUI3 C service, now
exposed under `include/rzi/lorawan/`. Headers are the ABI. This page is the
function index and backend coverage.

See also: [lorawan-api.md](./lorawan-api.md),
[at-command-compatibility.md](./at-command-compatibility.md),
[rui3-gap.md](./rui3-gap.md).

## Purpose

These signatures are the RZI LoRaWAN ABI. Unsupported backends return
`-ENOTSUP`. The reference behavior is the RUI3 C service and C AT
implementation, not `component/rui_v3_api/RAKLorawan.*`. Persistent
OTAA/ABP credentials stay inputs to `rzi_lorawan_join()`. Join retry,
confirm-default, and last-payload readback stay in the AT package.
P2P/FSK is `include/rzi/lora/lora.h`, not this ABI.

`tx_power` is the LoRaWAN power index, not dBm. `sub_band` is 0 (all) or
1–8. `query_tx_possible()` returns `0` when `size` fits the current data
rate. Class B ping-slot periodicity is 0–7. Channel masks use up to
`RZI_LORAWAN_CHANNEL_MASK_WORDS` (6) words.

## `include/rzi/lorawan/lorawan.h`

Core lifecycle (SDK 0.2) plus network, channel, information, and Class B
(SDK 0.3). `rzi_lorawan_callbacks.on_event` reports link check as
`RZI_LORAWAN_EVENT_LINK_CHECK` and device time as
`RZI_LORAWAN_EVENT_DEVICE_TIME`.

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

## `include/rzi/lorawan/mac_commands.h`

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

## `include/rzi/lorawan/channel_scan.h`

```c
int rzi_lorawan_get_channel_rssi_count(size_t *count);
int rzi_lorawan_get_channel_rssi(size_t index, struct rzi_lorawan_channel_rssi *rssi);
```

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
backend implements that group. A NULL operation still returns `-ENOTSUP`.

| Group | USP / LBM | Zephyr `lorawan_*` |
|---|---|---|
| Join / send / class | OTAA, Class A/B/C | OTAA, ABP, Class A/C |
| ADR, DR, public network, LBT | Yes | ADR, DR |
| TX power, duty-cycle enable, RX / join windows | `-ENOTSUP` (not in `smtc_modem_*`) | `-ENOTSUP` |
| Channel mask / sub-band / fixed channel | `-ENOTSUP` | Channel mask |
| RSSI / SNR / protocol version | Core downlink cache / constant | Same |
| `query_tx_possible` / `is_busy` / DevNonce | TX size and busy | TX size, busy, DevNonce |
| Class B ping-slot / state | Yes; beacon fields `-ENOTSUP` | `-ENOTSUP` |
| LinkCheckReq / DeviceTimeReq | Yes | Yes (not under `CONFIG_LORAWAN_EMUL`) |
| Multicast 0–3 | Yes | `-ENOTSUP` |
| Channel RSSI scan | `-ENOTSUP` | `-ENOTSUP` |
| Certification | Mode yes; FPort is local | `-ENOTSUP` |
| FUOTA | When `CONFIG_RZI_LORAWAN_FUOTA` | When enabled |

AT commands wrap these APIs. `-ENOTSUP` becomes `AT_ERROR`.

## Out of this ABI

Do not add these as `rzi_lorawan_*`:

- credential get/set (`dev_eui`, `join_eui`, `app_key`, ABP keys) — `join()`
- confirm default, last confirm status, send retry — AT `CFM` / `CFS` / `RETY`
- auto-join period and attempt count — AT `JOIN`
- last received payload buffer — AT `RECV`; C uses `downlink`
- work-mode switch to P2P/FSK — `rzi_lora_*`
- sleep / suspend / resume — `rzi_power_*`
- LPTP, LmHandler, LoRaMac types
