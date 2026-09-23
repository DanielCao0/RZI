# LoRaWAN API

Status: normative

Public C contract between applications and the selected protocol backend.
The precise ABI is `include/rzi/lorawan/lorawan.h` (group 0.3.0) plus
`mac_commands.h`, `multicast.h`, `channel_scan.h`, `certification.h`, and
`fuota.h`.

See also: [lorawan-backends.md](./lorawan-backends.md),
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

Class B, network/channel management, and information stay in LoRaWAN Core.
DeviceTimeReq and LinkCheckReq stay together under
`src/lorawan/mac_commands/`. Channel scan, multicast, certification, and
FUOTA live under `src/lorawan/services/`. Clock Synchronization, Remote
Multicast Setup, Fragmentation, and the Firmware Management Package
required by FUOTA come from the selected backend. RZI does not copy those
package implementations.

Public facades live in `include/rzi/lorawan/`. Private backend operation
tables stay out of the public include path. A backend advertises runtime
support with capability bits; a missing or NULL operation returns
`-ENOTSUP`. FUOTA remains Kconfig-gated. Raw LoRa P2P and FSK use the
separate `CONFIG_RZI_LORA` and `include/rzi/lora/lora.h`.

## 2. Relationship to Zephyr and RUI3

RZI keeps Zephyr's main operation order and concepts:

```text
set_region -> start -> join(config) -> send(port, data, size, type)
```

- `join` takes a separate activation config;
- `send` uses a confirmed/unconfirmed message type;
- Region, activation, device class, and message type are explicit enums;
- Standard capabilities the backend does not support return `-ENOTSUP`.

RZI does not copy Zephyr's synchronous join semantics. USP/LBM join, TX, and
downlink are already asynchronous events, so RZI delivers one
`on_event()` call per queued event. That avoids long blocks on the caller
thread and does not invent a synchronous result.

## 3. Lifecycle

Typical call order:

```c
static const struct rzi_lorawan_callbacks callbacks = {
    .on_event = on_event,
    .user_data = context,
};

rzi_lorawan_register_callbacks(&callbacks, &handle);
rzi_lorawan_set_region(RZI_LORAWAN_REGION_EU_868);
rzi_lorawan_start();
/* after on_event(RZI_LORAWAN_EVENT_READY) */
rzi_lorawan_join(&join_config);
```

Rules:

1. Callbacks may be registered before `start`. Doing so is recommended so
   `RZI_LORAWAN_EVENT_READY` is not missed;
2. `set_region` and `set_join_backoff_bypass` may be called only before
   `start`;
3. `start` is a process-wide one-shot. A repeat returns `-EALREADY`;
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
means the backend accepted the request. `on_event()` with
`RZI_LORAWAN_EVENT_JOINED` means network activation finished.
`RZI_LORAWAN_EVENT_JOIN_FAILED` carries a negative errno when this attempt
ended without joining.

Retry policy belongs to the application or the AT service. RZI core does
not hide an infinite retry loop.

## 5. Uplink and downlink

`rzi_lorawan_send()` ports are 1 through 223. Maximum payload is
`RZI_LORAWAN_MAX_PAYLOAD`. The backend copies the payload before the
function returns. Only one outstanding uplink is allowed. A second request
returns `-EBUSY`.

`RZI_LORAWAN_EVENT_TX_DONE` carries a `rzi_lorawan_tx_result` that is valid
only for the duration of `on_event()`:

- `RZI_LORAWAN_TX_ACKED`: a confirmed uplink received an acknowledgement;
- `RZI_LORAWAN_TX_SENT`: the frame was sent without an acknowledgement;
- `RZI_LORAWAN_TX_NOT_SENT`: the request was never sent.

Metadata and payload pointers in `RZI_LORAWAN_EVENT_DOWNLINK` are valid only
until `on_event()` returns. Subscribers that need the data later must copy it.

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
`on_event()` type `RZI_LORAWAN_EVENT_ERROR` and errno `-EOVERFLOW`. Queue depth, subscriber limit, dispatcher stack, and
priority are Kconfig settings.

The callback table, including `user_data`, is copied at registration. The
object `user_data` points to remains owned by the caller. Unregistration
stops new dispatch snapshots from including that subscriber, but a callback
already in flight may still run. The caller must free `user_data` only
after every in-flight callback has returned.

## 7. Error semantics

Every fallible API returns `0` or a negative errno:

- `-EINVAL`: invalid argument, enum, port, or payload length;
- `-EWOULDBLOCK`: a thread-context API was called from an ISR;
- `-EAGAIN`: the service or backend is not ready;
- `-EALREADY`: repeated `start`, or a start-time setting changed after
  `start`;
- `-EBUSY`: a conflicting asynchronous operation is already in flight;
- `-ENOTSUP`: the selected backend does not support the requested
  capability;
- `-ENOMEM`: callback subscriber slots are full;
- `-EIO`: a backend failure without a more precise error.

The `error` callback reports asynchronous backend errors and event-queue
overflow. It does not replace synchronous API argument checks.

## 8. Backend contract

Private `src/lorawan/backend/lorawan_backend.h` defines the backend vtable
and the internal event envelope. A backend:

- provides a capability bit mask;
- receives region, development policy, and an event sink at `start`;
- deep-copies the join config and send payload;
- converts vendor return values to negative errno;
- does not call user callbacks directly;
- does not leak vendor objects, enums, or thread models into public
  headers.

`src/lorawan/backend/lorawan_feature.h` defines versioned extension
descriptors for optional capabilities. Each feature defines a typed
operations table in its own private header and obtains it from the backend
by feature ID. `size` and `version` are used for compatibility checks. A
public capability bit means runtime support. Core public APIs are always
compiled except FUOTA, which remains Kconfig-gated. Neither replaces the
other.

A new backend only needs to implement the core contract and provide
extensions for the capabilities it actually has. Applications should not
have to change the public call flow.

## 9. Compatibility policy

This interface replaces the early v0.1 `rzi_lorawan_init(config)` and
`rzi_lorawan_get_event()`. RZI has not published a stable 1.0 ABI, so it
does not keep compatibility wrappers that would create dual event consumers
or ambiguous semantics. Later breaking public ABI changes must:

1. Update this document and the public header;
2. Update every RZI sample, the AT service, and integrated applications;
3. Pass style checks, unit tests, and at least one real backend build;
4. State the migration in the release note.
