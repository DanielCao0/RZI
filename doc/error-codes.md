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
`enum rzi_err` value. The integers are part of the 0.x ABI and must not
alias POSIX errno numbers. Zephyr's minimal libc defines
`EWOULDBLOCK` as `EAGAIN`, so those POSIX names cannot distinguish an ISR
call from a service that is not ready.

`rzi_err_str()` accepts `0`, a positive catalog value, or the negative
value returned by an API.

## Catalog

| Code | Value | Meaning |
|---|---|---|
| `RZI_SUCCESS` | 0 | Request accepted or operation completed |
| `RZI_ERR_INVALID` | 1 | Invalid argument, enum, port, or payload field |
| `RZI_ERR_WOULDBLOCK` | 2 | Thread-context API called from an ISR |
| `RZI_ERR_NOT_READY` | 3 | Service or backend has not started |
| `RZI_ERR_ALREADY` | 4 | Repeated start, or a start-time setting changed after start |
| `RZI_ERR_BUSY` | 5 | Conflicting asynchronous operation in flight |
| `RZI_ERR_NOT_SUPPORTED` | 6 | Selected backend cannot perform the request |
| `RZI_ERR_NO_RESOURCE` | 7 | Subscriber slot, buffer, or table is full |
| `RZI_ERR_NOT_FOUND` | 8 | Handle, key, session, or image does not exist |
| `RZI_ERR_NO_DATA` | 9 | No downlink, beacon, or reconstructed image size |
| `RZI_ERR_TOO_LARGE` | 10 | Payload, key, or stored value exceeds the accepted size |
| `RZI_ERR_TIMEOUT` | 11 | Operation timed out |
| `RZI_ERR_OVERFLOW` | 12 | Event or input queue overflowed |
| `RZI_ERR_IO` | 13 | Backend or hardware failure without a more precise code |
| `RZI_ERR_BAD_MESSAGE` | 14 | CRC or protocol contents are damaged |
| `RZI_ERR_NO_DEVICE` | 15 | Required device is missing or not ready |
| `RZI_ERR_NOT_JOINED` | 16 | Request requires an active network session |
| `RZI_ERR_DENIED` | 17 | Caller is not allowed to perform this operation |

Each public function documents the subset it actually returns. Asynchronous
callbacks use the same catalog: `join_done`, `error`, `device_time_done`,
and `tx_done` receive `0` or a negative `RZI_ERR_*`. `send_done` stores
that value in `rzi_lorawan_tx_result.error`.

## Backend mapping

USP, Zephyr LoRaWAN, flash, settings, and UART return values are converted
at the service or backend boundary. Public functions must not forward a
vendor errno. Unknown vendor codes become `-RZI_ERR_IO`.

`rzi_err_from_errno()` is the last-resort mapper for a driver boundary. It
is not a substitute for explicit argument checks. Because `EAGAIN` and
`EWOULDBLOCK` are aliases on Zephyr, that helper maps both to
`-RZI_ERR_NOT_READY`. ISR rejection stays `-RZI_ERR_WOULDBLOCK` and is
decided by RZI before a backend is entered.

## AT mapping

AT command handlers return the same closed codes as the public C APIs.
The parser maps them to unchanged RUI3 status names:

| Handler return | AT status |
|---|---|
| `0` after a handler-written reply | no extra status |
| `-RZI_ERR_INVALID` | `AT_PARAM_ERROR` |
| `-RZI_ERR_BUSY` | `AT_BUSY_ERROR` |
| `-RZI_ERR_NOT_JOINED` | `AT_NO_NETWORK_JOINED` |
| any other non-zero | `AT_ERROR` |

`AT_TEST_PARAM_OVERFLOW` is written by the line parser when input exceeds
`CONFIG_RZI_AT_LINE_MAX`. It is not produced from this table. Asynchronous
AT events keep their RUI3 names, including `+EVT:JOIN_FAILED_RX_TIMEOUT`.
