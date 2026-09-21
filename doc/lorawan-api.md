# LoRaWAN API

Status: normative

Public C contract between applications and the selected protocol backend.
The precise ABI is `include/rzi/lorawan/lorawan.h` (group 0.4.0) plus
`multicast.h`, `certification.h`, and `fuota.h`.

See also: [error-codes.md](./error-codes.md),
[lorawan-backends.md](./lorawan-backends.md),
[fuota.md](./fuota.md), [rui3-mapping.md](./rui3-mapping.md).
The header-to-RUI3 mapping is in [rui3-mapping.md](./rui3-mapping.md).

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

Class B, network/channel management, information, DeviceTimeReq, and
LinkCheckReq stay in LoRaWAN Core. Multicast and certification are also
core facades with dedicated public headers. FUOTA
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

RZI keeps Zephyr's main operation order and concepts:

```text
set_region -> start -> join(config) -> send(port, data, size, type)
```

- `join` takes a separate activation config;
- `send` uses a confirmed/unconfirmed message type;
- Region, activation, device class, and message type are explicit enums;
- Standard capabilities the backend does not support return `-RZI_ERR_NOT_SUPPORTED`.

RZI does not copy Zephyr's synchronous join semantics. USP/LBM join, TX, and
downlink are already asynchronous events, so RZI uses RUI3-style
`join_done`, `send_done`, `downlink`, and `state_changed` callbacks. That
avoids long blocks on the caller thread and does not invent a synchronous
result.

## 3. Lifecycle

Typical call order:

```c
static const struct rzi_lorawan_callbacks callbacks = {
    .join_done = on_join_done,
    .send_done = on_send_done,
    .downlink = on_downlink,
    .state_changed = on_state_changed,
    .error = on_error,
    .user_data = context,
};

rzi_lorawan_register_callbacks(&callbacks, &handle);
rzi_lorawan_set_region(RZI_LORAWAN_REGION_EU_868);
rzi_lorawan_start();
/* after state_changed(RZI_LORAWAN_STATE_READY) */
rzi_lorawan_join(&join_config);
```

Rules:

1. Callbacks may be registered before `start`. Doing so is recommended so
   the `READY` state is not missed;
2. `set_region` and `set_join_backoff_bypass` may be called only before
   `start`;
3. `start` is a process-wide one-shot. A repeat returns `-RZI_ERR_ALREADY`;
4. `RZI_LORAWAN_STATE_READY` means the backend can accept a join. It does
   not mean the device has joined the network;
5. After an unexpected modem reset, RZI reconfigures the backend and emits
   `READY` again;
6. This version has no stop/reinit. Fatal errors after the backend has
   started should be logged by the application, then the device rebooted.

## 4. Activation

`rzi_lorawan_join_config` aligns with Zephyr `lorawan_join_config` and uses
an activation enum plus a union:

- OTAA: `dev_eui`, `join_eui`, `network_key`, `application_key`.
  The current USP backend maps `network_key` to the LoRaWAN 1.0.x AppKey
  and `application_key` to **GenAppKey** (the ChirpStack FUOTA multicast /
  fragmentation root key);
- ABP: `dev_addr`, `network_session_key`, `application_session_key`.

The public API can express OTAA and ABP. Actual support is a capability.
Applications should check `rzi_lorawan_get_capabilities()`.

| Backend | Default capabilities | With `CONFIG_RZI_LORAWAN_FUOTA` |
|---|---|---|
| USP | OTAA, Class A/B/C, multicast, link check, information, network management, device time, certification | Adds FUOTA |
| Zephyr | OTAA, ABP, Class A/C, link check, information, network/channel management, device time | Adds FUOTA |

AT `AT+APPKEY` copies the same 16 bytes into both `network_key` and
`application_key`. Native C callers that need a separate ChirpStack
GenAppKey must fill the two fields themselves; the AT package cannot.

The join config is deep-copied before `rzi_lorawan_join()` returns. The
caller may then free or mutate the original object. A return of `0` only
means the backend accepted the request. `join_done` status `0` means
network activation finished. A negative `RZI_ERR_*` means this attempt ended
without joining.

Retry policy belongs to the application or the AT service. RZI core does
not hide an infinite retry loop.

## 5. Uplink and downlink

`rzi_lorawan_send()` ports are 1 through 223. Maximum payload is
`RZI_LORAWAN_MAX_PAYLOAD`. A larger payload returns `-RZI_ERR_TOO_LARGE`,
the same code `rzi_lorawan_query_tx_possible()` uses when the current data
rate cannot carry the frame. The backend copies the payload before the
function returns. Only one outstanding uplink is allowed. A second request
returns `-RZI_ERR_BUSY`. Unjoined devices return `-RZI_ERR_NOT_JOINED`.

`send_done` receives a `rzi_lorawan_tx_result` that is valid only for the
duration of the callback:

- `RZI_LORAWAN_TX_ACKED`: a confirmed uplink received an acknowledgement;
- `RZI_LORAWAN_TX_SENT`: the frame was sent without an acknowledgement;
- `RZI_LORAWAN_TX_NOT_SENT`: the request was never sent; `error` holds the
  negative `RZI_ERR_*` from the backend.

Metadata and payload pointers in the downlink callback are valid only until
that callback returns. Subscribers that need the data later must copy it.

`enum rzi_lorawan_data_rate` names the 4-bit MAC index (0-15). Core rejects
values that the current region does not define:

| Region | Uplink (`set_data_rate`) | RX2 / multicast |
|---|---|---|
| EU868 | 0-11 | 0-7 |
| US915 | 0-6 | 8-13 |
| AU915 | 0-7 | 8-13 |
| CN470 | 0-7 | 0-7 |
| AS923 groups 1-4 | 0-7 | 0-7 |
| IN865 | 0-5, 7 | 0-5, 7 |
| KR920 | 0-5 | 0-5 |
| RU864 | 0-7 | 0-7 |

DR14 and DR15 are unused in every region RZI exposes. EU868 uplink 8-11
and US915/AU915 uplink 5-7 are LR-FHSS indexes from RP2 1.0.3.

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
subscriber delays the others. On queue overflow, RZI reports the loss with
`error(-RZI_ERR_OVERFLOW)`. Queue depth, subscriber limit, dispatcher stack, and
priority are Kconfig settings.

The callback table, including `user_data`, is copied at registration. The
object `user_data` points to remains owned by the caller. Unregistration
stops new dispatch snapshots from including that subscriber, but a callback
already in flight may still run. The caller must free `user_data` only
after every in-flight callback has returned.

## 7. Error semantics

Every fallible API returns `0` or a negative `enum rzi_err`. The closed
catalog and AT mapping are in [error-codes.md](./error-codes.md). LoRaWAN
uses this subset:

- `-RZI_ERR_INVALID`: invalid argument, enum, or port;
- `-RZI_ERR_WOULDBLOCK`: a thread-context API was called from an ISR;
- `-RZI_ERR_NOT_READY`: the service or backend is not ready;
- `-RZI_ERR_ALREADY`: repeated `start`, or a start-time setting changed
  after `start`;
- `-RZI_ERR_BUSY`: a conflicting asynchronous operation is already in
  flight;
- `-RZI_ERR_NOT_SUPPORTED`: the selected backend does not support the
  requested capability;
- `-RZI_ERR_NO_RESOURCE`: callback subscriber slots are full;
- `-RZI_ERR_NOT_FOUND`: handle or multicast session does not exist;
- `-RZI_ERR_NO_DATA`: no downlink, beacon, or modem time/event is available;
- `-RZI_ERR_TOO_LARGE`: the payload exceeds the API maximum or the current
  data rate;
- `-RZI_ERR_TIMEOUT`: join retries were exhausted, or a MAC request timed
  out, and the backend has no more precise code;
- `-RZI_ERR_OVERFLOW`: the dispatcher event queue overflowed;
- `-RZI_ERR_IO`: a backend failure without a more precise error;
- `-RZI_ERR_NOT_JOINED`: uplink requested without an active session.

The `error` callback reports asynchronous backend errors and event-queue
overflow. It does not replace synchronous API argument checks.

## 8. Backend contract

Private `src/lorawan/backend/lorawan_backend.h` defines the backend vtable
and the internal event envelope. A backend:

- provides a capability bit mask;
- receives region, development policy, and an event sink at `start`;
- deep-copies the join config and send payload;
- converts vendor return values to negative `RZI_ERR_*`;
- does not call user callbacks directly;
- does not leak vendor objects, enums, or thread models into public
  headers.

`src/lorawan/backend/lorawan_backend.h` hangs typed ops tables on the
backend contract. A NULL table means the backend does not implement that
group; a NULL member still returns `-RZI_ERR_NOT_SUPPORTED`. A public
capability bit advertises runtime support and must stay aligned with the
non-NULL pointers. Core public APIs are always compiled except FUOTA, which
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
