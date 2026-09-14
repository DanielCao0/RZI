# Storage API

Status: implemented

`include/rzi/storage/storage.h` is the only persistent key-value interface
used by RZI services. Service and AT code must not call Zephyr settings,
NVS, or flash APIs directly.

See also: [at-command-compatibility.md](./at-command-compatibility.md),
[architecture.md](./architecture.md) §10.

The Zephyr adapter is `src/storage/storage_settings.c`.
`CONFIG_RZI_STORAGE` enables the service; the application still selects
the Zephyr settings backend. Write and delete take `RZI_POWER_BLOCK_FLASH`
when auto-block is on.

## Contract

- `rzi_storage_init()` is idempotent and thread-context only.
- Keys are composed as `<namespace>/<key>`.
- Read requires an exact stored-size match and returns `-EMSGSIZE`
  otherwise.
- Write replaces one complete value.
- Delete returns `-ENOENT` when no value exists.
- Backend operations are serialized by the service.
- Empty, absolute, trailing-slash, and double-slash paths are rejected.
- Composed names longer than 63 characters return `-ENAMETOOLONG`.

Namespaces belong to services. A service must not read, rewrite, or delete
another service's namespace.

The header does not return `-EWOULDBLOCK`; callers must stay in thread
context.

## AT keys

The LoRaWAN AT package uses namespace `rzi` and these keys, so firmware
that already stored credentials stays compatible:

| Key | Content |
|---|---|
| `deveui` | 8-byte DevEUI |
| `joineui` | 8-byte JoinEUI |
| `appkey` | 16-byte AppKey |
| `band` | RUI3 band number |
| `cfm` | Confirmed-uplink flag |
| `autojoin` | Auto-join after boot |
| `join_interval` | Retry interval in seconds |
| `join_attempts` | Retry count after the first attempt |

## Security and future work

The API stores opaque bytes. It does not claim encryption, secure-key
storage, transactions, or schema migration.

Before a stable 1.0 release, storage must add schema version metadata,
transactional multi-key updates where required, migration tests, a
coordinated factory-reset policy, and a secure credential profile.
