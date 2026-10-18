# Error codes

Status: normative

Closed public failure catalog for every fallible `rzi_*` C API. The ABI is
`include/rzi/err.h` (group 0.4.0).

See also: [coding-standards.md](./coding-standards.md),
[architecture.md](./architecture.md),
[at-framework.md](./at-framework.md),
[lorawan-api.md](./lorawan-api.md).

## Return convention

Public functions return `0` (`RZI_SUCCESS`) or the negation of one
`enum rzi_err` value. Catalog integers start at 1000, above Zephyr errno,
so a leaked `-EIO` or `-EBUSY` cannot alias a public RZI code.
`rzi_err_str()` reports a leaked errno as `RZI_ERR_UNKNOWN`.

Zephyr's minimal libc defines `EWOULDBLOCK` as `EAGAIN`, so those POSIX
names cannot distinguish an ISR call from a service that is not ready.
ISR rejection is `RZI_ERR_ISR` and is decided by RZI before a backend is
entered.

`rzi_err_str()` accepts `0`, a positive catalog value, or the negative
value returned by an API.

## Catalog

| Code | Value | Meaning |
|---|---|---|
| `RZI_SUCCESS` | 0 | Request accepted or operation completed |
| `RZI_ERR_INVALID` | 1000 | Invalid argument, enum, port, or payload field |
| `RZI_ERR_ISR` | 1001 | Thread-context API called from an ISR |
| `RZI_ERR_NOT_READY` | 1002 | Service or backend has not started |
| `RZI_ERR_ALREADY` | 1003 | Repeated start, or the object is already registered |
| `RZI_ERR_BUSY` | 1004 | Conflicting asynchronous operation in flight |
| `RZI_ERR_NOT_SUPPORTED` | 1005 | Selected backend cannot perform the request |
| `RZI_ERR_NO_RESOURCE` | 1006 | Subscriber slot, buffer, or table is full |
| `RZI_ERR_NOT_FOUND` | 1007 | Handle, key, session, or image does not exist |
| `RZI_ERR_NO_DATA` | 1008 | No downlink, beacon, or reconstructed image size |
| `RZI_ERR_TOO_LARGE` | 1009 | Payload, key, or stored value exceeds the accepted size |
| `RZI_ERR_TIMEOUT` | 1010 | Operation timed out |
| `RZI_ERR_OVERFLOW` | 1011 | Event or input queue overflowed |
| `RZI_ERR_IO` | 1012 | Backend or hardware failure without a more precise code |
| `RZI_ERR_BAD_MESSAGE` | 1013 | CRC or protocol contents are damaged |
| `RZI_ERR_NO_DEVICE` | 1014 | Required device is missing |
| `RZI_ERR_NOT_JOINED` | 1015 | Request requires an active network session |
| `RZI_ERR_DENIED` | 1016 | Caller is not allowed to perform this operation |
| `RZI_ERR_INVALID_STATE` | 1017 | Service lifecycle does not allow this operation |

`RZI_ERR_ALREADY` means the requested state or object is already present:
a second `init` or `start`, or a duplicate AT command name. A setting
changed after `rzi_lorawan_init`, and registration after the AT registry is sealed,
return `RZI_ERR_INVALID_STATE`. `RZI_ERR_NO_DEVICE` means the hardware
device is missing. An adapter or service that has not been started returns
`RZI_ERR_NOT_READY`.

Each public function documents the subset it actually returns. Asynchronous
events use the same catalog: `JOIN_FAILED`, `ERROR`, and `DEVICE_TIME` carry
`0` or a negative `RZI_ERR_*` in `rzi_lorawan_event.error`; `TX_DONE` stores
that value in `rzi_lorawan_tx_result.error`.

## Backend mapping

USP, Zephyr LoRaWAN, flash, settings, and UART return values are converted
at the service or backend boundary. Public functions must not forward a
vendor errno. Unknown vendor codes become `-RZI_ERR_IO`.

`rzi_err_from_errno()` is the last-resort mapper for a driver boundary. It
is not a substitute for explicit argument checks. A negative value that is
already in the catalog is returned unchanged, so mapping twice does not
change the code. Because `EAGAIN` and `EWOULDBLOCK` are aliases on Zephyr,
that helper maps both to `-RZI_ERR_NOT_READY`. ISR rejection stays
`-RZI_ERR_ISR` and is decided by RZI before a backend is entered.
`ENOSYS` becomes `-RZI_ERR_NOT_SUPPORTED`, `EEXIST` becomes
`-RZI_ERR_ALREADY`, `EPERM`/`EACCES` become `-RZI_ERR_DENIED`, `ENOSPC`
becomes `-RZI_ERR_NO_RESOURCE`, and `ENETDOWN` becomes
`-RZI_ERR_NOT_READY`.

The generic mapper does not produce `-RZI_ERR_NOT_JOINED`. A LoRaWAN
backend that knows the stack uses `ENETDOWN` for "no session" converts that
value itself. The Zephyr LoRaWAN backend does this before calling
`rzi_err_from_errno()`.

## AT mapping

AT command handlers return the same closed codes as the public C APIs.
The parser maps them to unchanged RUI3 status names:

| Handler return | AT status |
|---|---|
| `0` after a handler-written reply | no extra status |
| `-RZI_ERR_INVALID` | `AT_PARAM_ERROR` |
| `-RZI_ERR_BUSY` | `AT_BUSY_ERROR` |
| `-RZI_ERR_NOT_JOINED` | `AT_NO_NETWORK_JOINED` |
| `-RZI_ERR_TOO_LARGE` | `AT_TEST_PARAM_OVERFLOW` |
| `-RZI_ERR_OVERFLOW` | `AT_TEST_PARAM_OVERFLOW` |
| any other non-zero | `AT_ERROR` |

The line parser also writes `AT_TEST_PARAM_OVERFLOW` when input exceeds
`CONFIG_RZI_AT_LINE_MAX`. Asynchronous AT events keep their RUI3 names,
including `+EVT:JOIN_FAILED_RX_TIMEOUT`.
