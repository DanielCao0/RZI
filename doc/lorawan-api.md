# LoRaWAN API

Status: normative

Public C contract between applications and the selected protocol backend.
The precise ABI is `include/rzi/lorawan/lorawan.h` (group 0.4.0) plus
`multicast.h`, `certification.h`, and `fuota.h`.

See also: [error-codes.md](./error-codes.md),
[lorawan-backends.md](./lorawan-backends.md),
[fuota.md](./fuota.md), [rui3-mapping.md](./rui3-mapping.md).

## 1. Purpose

The RZI LoRaWAN API is the stable C interface between applications and a
concrete protocol stack. Public interfaces follow these rules:

- Operation shape stays close to the Zephyr LoRaWAN API so Zephyr developers
  can migrate with low cost;
- Asynchronous results use RUI3-style callbacks instead of making the
  application poll an internal queue;
- Public types do not expose USP, LoRa Basics Modem, or other backend types;
- One backend and one modem instance, with multiple independent callback
  subscribers;
- No heap. RZI copies all data that crosses threads;
- API return values only say whether a request was accepted. Final results
  arrive through callbacks.

Class B, network/channel management, last-downlink RSSI/SNR, DeviceTimeReq,
and LinkCheckReq stay in the LoRaWAN service. Multicast and certification are
also service facades with dedicated public headers. FUOTA
lives under `src/lorawan/services/fuota/`. Clock Synchronization, Remote
Multicast Setup, Fragmentation, and the Firmware Management Package
required by FUOTA come from the selected backend. RZI does not copy those
package implementations.

Public facades live in `include/rzi/lorawan/`. Private backend operation
tables stay out of the public include path. A backend advertises runtime
support with capability bits; a missing or NULL operation returns
`-RZI_ERR_NOT_SUPPORTED`. FUOTA remains Kconfig-gated. Raw LoRa P2P and FSK use the
separate `CONFIG_RZI_LORA` and `include/rzi/lora/lora.h`.

## 2. Relationship to Zephyr and RUI3

RZI keeps Zephyr's main operation order and uses RUI3's credential and join shape:

```text
set credentials -> init(region) -> join(start, auto_join, interval, attempts) -> send
```

- DevEUI, AppEUI, AppKey, GenAppKey, and ABP credentials are stored before join;
- `join` uses those stored values. `RZI_LORAWAN_JOIN_KEEP` (`-1`) leaves one argument unchanged;
- `send` uses a confirmed/unconfirmed message type;
- Region, activation, device class, and message type are explicit enums;
- Standard capabilities the backend does not support return `-RZI_ERR_NOT_SUPPORTED`.

RZI does not copy Zephyr's synchronous join semantics. USP/LBM join, TX, and
downlink are already asynchronous events, so RZI delivers one `on_event()`
call per queued public event. That avoids long blocks on the caller thread
and does not invent a synchronous result.

## Shared configuration and persistence

The public LoRaWAN service owns shared configuration. AT commands call the
same setters and getters as C applications; they do not maintain a second
copy of radio settings. Enable `CONFIG_RZI_STORAGE=y` and select a Zephyr
settings backend (for example `CONFIG_NVS=y`, `CONFIG_FLASH_MAP=y`, and
`CONFIG_SETTINGS_NVS=y`) to retain configuration across reboots. Without
storage, the same APIs keep settings in RAM. `CONFIG_RZI_AT_NVM` is only
needed for AT-owned system settings and also enables the public storage service.

| Setting | Public API | AT command |
| --- | --- | --- |
| Region | `rzi_lorawan_get/set_region` | `BAND` |
| Default confirmation mode | `rzi_lorawan_get/set_confirmed` | `CFM` |
| Retry policy, 0–7 | `rzi_lorawan_get/set_retries` | `RETY` |
| Configured NetID fallback | `rzi_lorawan_get/set_net_id` | `NETID` |
| Credentials, activation, join policy, channel mask | Existing credential/join/channel APIs | Corresponding commands |

Storage errors propagate to both API and AT callers. The four shared setters
above commit storage before publishing a new RAM value. New values use the
`lorawan` namespace. Existing `rzi/band`, `rzi/cfm`, `rzi/rety`, and `rzi/netid`
values migrate on first read; new service values take precedence.

`rzi_lorawan_get_region()` restores the configured region before initialization.
To start with it, read the region and pass it to `rzi_lorawan_init(region)`.
An explicit `init(region)` overrides and persists that region; applications
that hardcode a region deliberately replace the saved selection.

`rzi_lorawan_send()` still accepts an explicit confirmed/unconfirmed type;
call `get_confirmed()` when the application wants the stored default. `RETY`
remains stored policy only, as before: it does not implement backend retries
or replace `NbTrans`. Configuring NetID does not change a joined network's
NetID; `get_net_id()` prefers the active session and otherwise reads the fallback.

`rzi_lorawan_clear_settings()` deletes credentials and shared settings in both
namespaces. Reboot after success to discard runtime state and reload defaults.
AT factory reset delegates to this API. Protocol-stack contexts, including
DevNonce, remain owned by the backend. Other setters such as ADR, class and
P2P radio configuration retain their existing runtime-only behavior.

## 3. Lifecycle

Typical call order:

```c
static const struct rzi_lorawan_callbacks callbacks = {
    .on_event = on_event,
    .user_data = context,
};

rzi_lorawan_register_callbacks(&callbacks);
rzi_lorawan_set_dev_eui(dev_eui, sizeof(dev_eui));
rzi_lorawan_set_app_eui(app_eui, sizeof(app_eui));
rzi_lorawan_set_app_key(app_key, sizeof(app_key));
rzi_lorawan_set_activation(RZI_LORAWAN_ACTIVATION_OTAA);
rzi_lorawan_init(RZI_LORAWAN_REGION_EU_868);
/* after on_event(RZI_LORAWAN_EVENT_READY) */
rzi_lorawan_join(1, 0, RZI_LORAWAN_JOIN_INTERVAL_DEFAULT, 0);
```

Rules:

1. Callbacks may be registered before `init`. Doing so is recommended so
   `RZI_LORAWAN_EVENT_READY` is not missed;
2. `set_region` stores a region for channel-mask commands before `init`.
   `set_join_backoff_bypass` may be called only before `init`. After
   `init` both return `-RZI_ERR_INVALID_STATE`. `init` overwrites the
   stored region;
3. `init` is a process-wide one-shot. A repeat returns `-RZI_ERR_ALREADY`;
4. `RZI_LORAWAN_EVENT_READY` means the backend can accept a join. It does
   not mean the device has joined the network;
5. After an unexpected modem reset, RZI reconfigures the backend and emits
   `READY` again;
6. This version has no stop/reinit. Fatal errors after the backend has
   started should be logged by the application, then the device rebooted.

`READY`, `JOINED`, and `JOIN_FAILED` are dedicated completion events.
`STATE_CHANGED` carries service-originated transitions such as `STARTING`,
`JOINING`, `STOPPED`, and the return to `READY` after leaving. A backend join
completion produces one public event; it does not also produce a duplicate
`STATE_CHANGED` event.

### Join-backoff status

After `READY`, applications may query
`rzi_lorawan_get_join_backoff(uint32_t *remaining_s)` before, during, or after
joining. USP/LBM reports the remaining join-backoff restriction in seconds,
using LBM's seconds clock. An expired deadline, a joined session, or an
active bypass/certification mode returns zero. Query failures leave the
output unchanged. Backends without this operation return
`-RZI_ERR_NOT_SUPPORTED`; the Zephyr backend currently has no implementation.

The value is a snapshot, not a scheduled transmit time. It excludes random
join delay, regional channel duty cycle, radio arbitration, and application
retry delay. Zero does not imply an active join request or immediate radio
availability. Use wording such as `Join backoff remaining: N s`, rather than
`Retrying in N s`. Do not use the value to bypass the stack's scheduling.

The USP adapter uses the internal LBM
`lorawan_api_next_join_time_second_get()` getter under the modem lock.
Its prototype is checked at configure time in `lbm_checks.cmake`, so an
upstream signature change fails the build. Applications include only the
public RZI header and do not depend on LBM types or headers.

## 4. Activation

Credentials and the join mode are stored with `rzi_lorawan_set_*` /
`rzi_lorawan_set_activation()` and applied on the next `rzi_lorawan_join()`. The
backend still receives one private join configuration:

- OTAA: DevEUI, AppEUI (JoinEUI), and AppKey. AppKey is both the LoRaWAN
  1.0.x AppKey and the NwkKey. `rzi_lorawan_set_gen_app_key()` stores the
  USP/LBM GenAppKey; when it is unset, join copies AppKey into that field.
  The Zephyr backend maps GenAppKey to its join `app_key`;
- ABP: `dev_addr`, NwkSKey, and AppSKey.

`rzi_lorawan_get_dev_addr()`, `rzi_lorawan_get_nwk_skey()`, and
`rzi_lorawan_get_app_skey()` follow RUI3. ABP returns the stored values.
OTAA returns zeros until the device has joined, then the address and
session keys from the active stack. DevAddr is host byte order; AT prints
it as big-endian hex. A secure element that cannot export keys, such as
the LR11xx crypto engine, returns `-RZI_ERR_NOT_SUPPORTED` for the two
session-key reads after an OTAA join. DevAddr is still read from the MAC.

`rzi_lorawan_join(start, auto_join, interval, attempts)` follows RUI3
`service_lora_join()`. `start` of 1 joins. `start` of 0 stops the join
sequence, including an attempt still waiting for Join-Accept, and does not
leave a joined session. `RZI_LORAWAN_JOIN_KEEP` leaves that argument
unchanged. The default attempt count is 0, meaning one attempt.
Subscribers see `RZI_LORAWAN_EVENT_JOIN_FAILED` only after those attempts
are exhausted.

The stack owns retry timing; the service owns the attempt count. After
`rzi_lorawan_join()` the backend keeps sending Join-Requests and reports
each failure. The service counts them and calls the backend leave operation
when `attempts` is exhausted or on `start` of 0. USP/LBM spaces attempts with
the LoRaWAN 1.0.4 join duty cycle (1 % of airtime in the first hour, then
0.1 % and 0.01 %, plus 0 to 5 s of jitter) and ignores `interval`.
`rzi_lorawan_set_join_backoff_bypass()` removes that backoff for test setups.
The Zephyr backend waits `interval` seconds (default 8) between attempts and
cannot abort a `lorawan_join()` already in progress; it drops that result.

The public API can express OTAA and ABP. A backend that does not implement
a mode returns `-RZI_ERR_NOT_SUPPORTED` from `rzi_lorawan_set_activation()` and
from `rzi_lorawan_join()`.
`rzi_lorawan_get_capabilities()` is optional discovery for portable code
and the AT package; typical applications do not need to call it.

| Backend | Default capabilities | With `CONFIG_RZI_LORAWAN_FUOTA` |
|---|---|---|
| USP | OTAA, Class A, NETWORK, SESSION, MAC, CERTIFICATION; Class B, Class C, and MULTICAST when the matching `CONFIG_LORA_BASICS_MODEM_*` option is enabled | Adds FUOTA |
| Zephyr | OTAA, ABP, Class A/C, NETWORK, CHANNEL, SESSION, MAC | Adds FUOTA |

`AT+APPKEY` stores one AppKey. Native C callers that need a separate
ChirpStack GenAppKey call `rzi_lorawan_set_gen_app_key()`. The AT package
does not expose that key.

Credential setters copy their input before returning. A return of `0` from
`rzi_lorawan_join()` only means the backend accepted the request.
`RZI_LORAWAN_EVENT_JOINED` means network activation finished.
`RZI_LORAWAN_EVENT_JOIN_FAILED` carries a negative `RZI_ERR_*` when the
configured attempts ended without joining.

When `CONFIG_RZI_STORAGE=y`, the service persists credentials and join
policy in the `lorawan` namespace. A missing key is copied once from the
older AT `rzi/<key>` entry. Without storage, the values stay in RAM.

## 5. Uplink and downlink

`rzi_lorawan_send()` ports are 1 through 223. Maximum payload is
`RZI_LORAWAN_MAX_PAYLOAD`. A larger payload returns `-RZI_ERR_TOO_LARGE`,
the same code `rzi_lorawan_query_tx_possible()` uses when the current data
rate cannot carry the frame. The backend copies the payload before the
function returns. Only one outstanding uplink is allowed. A second request
returns `-RZI_ERR_BUSY`. Unjoined devices return `-RZI_ERR_NOT_JOINED`.

`RZI_LORAWAN_EVENT_TXDONE` carries a `rzi_lorawan_tx_result` that is valid
only for the duration of `on_event()`:

- `RZI_LORAWAN_EVENT_TXDONE_CONFIRMED`: a confirmed uplink received an acknowledgment;
- `RZI_LORAWAN_EVENT_TXDONE_SENT`: the frame was sent without an acknowledgment;
- `RZI_LORAWAN_EVENT_TXDONE_NOT_SENT`: the request was never sent; `error` holds the
  negative `RZI_ERR_*` from the backend.

Metadata and payload pointers in `RZI_LORAWAN_EVENT_DOWNDATA` are valid only
until `on_event()` returns. Subscribers that need the data later must copy it.

`rzi_lorawan_send_empty()` sends an uplink without application payload,
optionally with an FPort, for example to flush pending MAC answers or open
class A receive windows. It follows the same joined and one-outstanding-uplink
rules and completes with `RZI_LORAWAN_EVENT_TXDONE`. `rzi_lorawan_is_busy()`
reports the stack's own view of whether an uplink is still in progress.

`enum rzi_lorawan_data_rate` names the 4-bit MAC index (0-15). The core does
not keep regional tables. `rzi_lorawan_set_data_rate()` accepts only the
uplink data rates the backend reports for the active region (USP:
`smtc_modem_get_enabled_datarates()`). Multicast data rates are checked
against 0-15 and validated by the stack.

`RZI_LORAWAN_EVENT_DOWNDATA` carries the reception metadata LoRa Basics Modem
reports: `window` (`RX1`, `RX2`, `RXC`, `RXB`, or `UNKNOWN`), `data_rate`,
`frequency_hz`, and `flags`:

- `RZI_LORAWAN_DOWNLINK_FPENDING`: the network has more data queued;
- `RZI_LORAWAN_DOWNLINK_TIME_UPDATED`: the frame carried a time update
  (Zephyr backend only);
- `RZI_LORAWAN_DOWNLINK_MULTICAST`: received in a multicast window;
  `multicast_group` holds the group index.

Two stack status events report conditions LoRa Basics Modem detects on its own:

- `RZI_LORAWAN_EVENT_REGIONAL_DUTY_CYCLE`: `duty_cycle_limited` is true while regional
  duty-cycle limits block transmission, false when they lift;
- `RZI_LORAWAN_EVENT_NO_DOWNLINK_THRESHOLD`: `no_downlink` holds
  `RZI_LORAWAN_EVENT_NO_RX_THRESHOLD_ADR_BACKOFF_END` and/or
  `RZI_LORAWAN_EVENT_NO_RX_THRESHOLD_USER_THRESHOLD` after too many uplinks without a
  downlink (see `rzi_lorawan_set_no_downlink_threshold()`).

## 5a. Link control

These functions map one-to-one onto LoRa Basics Modem controls. A backend
without the control returns `-RZI_ERR_NOT_SUPPORTED`.

- `rzi_lorawan_set_adr_profile()`: network-controlled ADR, the two LBM mobile
  profiles, or a custom distribution of
  `RZI_LORAWAN_DR_DISTRIBUTION_LENGTH` (16) slots. Each slot holds a data
  rate; the stack picks uniformly among slots, so the share of a data rate is
  the number of slots holding it. Slots with data rates the region does not
  allow are ignored. Requires an active session (`-RZI_ERR_NOT_JOINED`).
  `rzi_lorawan_set_adr(false)` and `rzi_lorawan_set_data_rate()` are shortcuts
  that fill every slot with one data rate;
- `rzi_lorawan_set_nb_trans()`: 1 through 15 transmissions per uplink; only
  valid while the profile is not network-controlled;
- `rzi_lorawan_set_adr_ack()`: ADR_ACK_LIMIT and ADR_ACK_DELAY, 2 through 127;
- `rzi_lorawan_set_join_data_rates()`: distribution used for Join-Requests;
  rejected with `-RZI_ERR_BUSY` while joining or joined;
- `rzi_lorawan_get_duty_cycle_status()`: remaining regional duty-cycle budget
  in milliseconds (negative while blocked);
- `rzi_lorawan_set_csma()` / `rzi_lorawan_set_csma_params()`: LBM CSMA,
  present when `CONFIG_LORA_BASICS_MODEM_CSMA` is enabled;
- `rzi_lorawan_get_lost_connection_count()`: uplinks since the last downlink;
- `rzi_lorawan_set_no_downlink_threshold()`: uplink count that raises
  `RZI_LORAWAN_EVENT_NO_DOWNLINK_THRESHOLD`;
- `rzi_lorawan_set_crystal_error()`: crystal error in ppm used to widen
  receive windows.

Parameters LoRa Basics Modem takes from the regional parameters and network
MAC commands stay in the header for RUI3 compatibility but return
`-RZI_ERR_NOT_SUPPORTED` on every current backend: TX power, duty-cycle
enable, RX1/RX2 delays, RX2 data rate and frequency, and join-accept delays.

US915 and AU915 uplink channels can be limited before join. The core stores
one 72-channel mask with the credentials (storage key `chmask`) and passes it
to the backend at the next join. `rzi_lorawan_set_sub_band()` (LoRaWAN FSB)
and `rzi_lorawan_set_fixed_channel()` both write that mask. A LinkADRReq from
the network replaces it until the next join. With USP this needs the
`usp/0003` LBM patch. CN470 and the other regions return
`-RZI_ERR_NOT_SUPPORTED`.

## 6. Callbacks and concurrency

A backend event callback only copies a complete event into a fixed-size RZI
message queue and returns immediately. The RZI dispatcher thread dequeues
events and calls every subscriber serially in registration order:

```text
USP/LBM callback -> private event queue -> RZI dispatcher -> subscribers
```

That guarantees user callbacks:

- do not run in an ISR;
- do not run inside the USP/LBM `rac_api_mutex`;
- do not run concurrently for the same event;
- may safely call non-blocking RZI LoRaWAN APIs.

Callbacks must return quickly and must not block for long. One slow
subscriber delays the others. On queue overflow, or when the stack itself
drops events (LBM `missed_events`), RZI reports the loss as
`RZI_LORAWAN_EVENT_ERROR` carrying `-RZI_ERR_OVERFLOW`. Queue depth,
subscriber limit, dispatcher stack, and priority are Kconfig settings.

The callback table, including `user_data`, is copied at registration. The
object `user_data` points to remains owned by the caller. A subscriber is
the pair of `on_event` and `user_data`. `rzi_lorawan_unregister_callbacks()`
takes a table with that same pair. Unregistration stops new dispatch
snapshots from including that subscriber, but a callback already in flight
may still run. The caller must free `user_data` only after every in-flight
callback has returned.

## 7. Error semantics

Every fallible API returns `0` or a negative `enum rzi_err`. The closed
catalog and AT mapping are in [error-codes.md](./error-codes.md). LoRaWAN
uses this subset:

- `-RZI_ERR_INVALID`: invalid argument, enum, or port;
- `-RZI_ERR_ISR`: a thread-context API was called from an ISR;
- `-RZI_ERR_NOT_READY`: the service or backend is not ready;
- `-RZI_ERR_ALREADY`: repeated `init`, or the same callback subscriber;
- `-RZI_ERR_INVALID_STATE`: a setting changed after `init`;
- `-RZI_ERR_BUSY`: a conflicting asynchronous operation is already in
  flight;
- `-RZI_ERR_NOT_SUPPORTED`: the selected backend does not support the
  requested capability;
- `-RZI_ERR_NO_RESOURCE`: callback subscriber slots are full;
- `-RZI_ERR_NOT_FOUND`: callback subscriber or multicast session does not exist;
- `-RZI_ERR_NO_DATA`: no downlink, beacon, or modem time/event is available;
- `-RZI_ERR_TOO_LARGE`: the payload exceeds the API maximum or the current
  data rate;
- `-RZI_ERR_TIMEOUT`: join retries were exhausted, or a MAC request timed
  out, and the backend has no more precise code;
- `-RZI_ERR_OVERFLOW`: the dispatcher event queue overflowed or the stack
  dropped events;
- `-RZI_ERR_IO`: a backend failure without a more precise error;
- `-RZI_ERR_NOT_JOINED`: uplink requested without an active session.

`RZI_LORAWAN_EVENT_ERROR` reports asynchronous backend errors and event-queue
overflow. It does not replace synchronous API argument checks.

## 8. Backend contract

Private `src/lorawan/backend/lorawan_backend.h` defines the backend vtable
and the internal event envelope. A backend:

- follows LoRa Basics Modem semantics; other stacks emulate them (see
  [lorawan-backends.md](./lorawan-backends.md));
- provides a capability bit mask and the mandatory `is_busy` operation;
- receives region, development policy, and an event sink at `init`;
- deep-copies the join config and send payload;
- converts vendor return values to negative `RZI_ERR_*`;
- does not call user callbacks directly;
- does not leak vendor objects, enums, or thread models into public
  headers.

`src/lorawan/backend/lorawan_backend.h` hangs typed ops tables on the
backend contract. A NULL table means the backend does not implement that
group; a NULL member still returns `-RZI_ERR_NOT_SUPPORTED`. A public
capability bit advertises runtime support and must stay aligned with the
non-NULL pointers; `rzi_lorawan_init()` asserts this with `CONFIG_ASSERT`. Core public APIs are always compiled except FUOTA, which
remains Kconfig-gated.

A new backend implements the lifecycle operations and fills in the tables it
has. Applications should not have to change the public call flow.

## 9. Compatibility policy

This interface replaces the early v0.1 `rzi_lorawan_init(config)` and
`rzi_lorawan_get_event()`. RZI has not published a stable 1.0 ABI, so it
does not keep compatibility wrappers that would create dual event consumers
or ambiguous semantics. Later breaking public ABI changes must:

1. Update this document and the public header;
2. Update every RZI sample, the AT service, and integrated applications;
3. Pass style checks, unit tests, and at least one real backend build;
4. State the migration in the release note.
