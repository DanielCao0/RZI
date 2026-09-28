# Storage API

Status: implemented

`include/rzi/storage/storage.h` is the only persistent key-value interface
used by RZI services. Service and AT code must not call Zephyr settings,
NVS, or flash APIs directly.

See also: [error-codes.md](./error-codes.md),
[at-command-compatibility.md](./at-command-compatibility.md),
[architecture.md](./architecture.md) §10.

The Zephyr adapter is `src/storage/storage_settings.c`.
`CONFIG_RZI_STORAGE` enables the service; the application still selects
the Zephyr settings backend. Write and delete take `RZI_POWER_BLOCK_FLASH`
when auto-block is on.

## Contract

- `rzi_storage_init()` is idempotent and thread-context only. An ISR
  caller returns `-RZI_ERR_WOULDBLOCK`.
- Keys are composed as `<namespace>/<key>`.
- Read requires an exact stored-size match and returns `-RZI_ERR_TOO_LARGE`
  otherwise.
- Write replaces one complete value.
- Delete returns `-RZI_ERR_NOT_FOUND` when no value exists.
- Backend operations are serialized by the service.
- Empty, absolute, trailing-slash, and double-slash paths are rejected.
- Composed names longer than 63 characters return `-RZI_ERR_TOO_LARGE`.
- Settings or flash failures are mapped to `-RZI_ERR_IO`.

Namespaces belong to services. A service must not read, rewrite, or delete
another service's namespace.

Callers must stay in thread context.

## LoRaWAN credentials

The LoRaWAN service uses namespace `lorawan` when `CONFIG_RZI_STORAGE=y`.
The first time a key is missing there, the service copies it from the older
AT namespace `rzi/<key>`.

| Key | Content |
|---|---|
| `deveui` | 8-byte DevEUI |
| `joineui` | 8-byte JoinEUI (RUI3 AppEUI) |
| `appkey` | 16-byte AppKey |
| `genappkey` | 16-byte GenAppKey, absent until set |
| `devaddr` | 4-byte ABP device address, host byte order |
| `nwkskey` | 16-byte ABP network session key |
| `appskey` | 16-byte ABP application session key |
| `njm` | `0` ABP, `1` OTAA |
| `autojoin` | Auto-join after boot |
| `join_interval` | Retry interval in seconds |
| `join_attempts` | Retry count after the first attempt |

`devaddr`, `nwkskey`, and `appskey` are the ABP values used by the next
ABP join. After an OTAA join, the matching getters read the stack instead
of these stored bytes.

## AT keys

The LoRaWAN AT package uses namespace `rzi` for settings that stay in AT:

| Key | Content |
|---|---|
| `band` | RUI3 band number |
| `cfm` | Confirmed-uplink flag |
| `rety` | Confirmed-uplink retry count |
| `netid` | Stored NetID |

## Security and future work

The API stores opaque bytes. It does not claim encryption, secure-key
storage, transactions, or schema migration.

Before a stable 1.0 release, storage must add schema version metadata,
transactional multi-key updates where required, migration tests, a
coordinated factory-reset policy, and a secure credential profile.
