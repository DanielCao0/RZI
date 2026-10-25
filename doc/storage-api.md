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
the Zephyr settings backend. Write and delete take `RZI_POWER_LOCK_FLASH`
when automatic service PM locks are enabled.

## Contract

- `rzi_storage_init()` is idempotent and thread-context only. An ISR
  caller returns `-RZI_ERR_ISR`.
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

## RAK3172 and RAK3172-F

Both boards reserve separate internal-flash areas for settings and the
LoRa Basics Modem context. Settings uses `storage_partition` at offset
`0x3c000` (8 KiB), while the modem uses `lora_context_partition` at
`0x3e000` (8 KiB). Offsets are relative to internal flash at `0x08000000`.
The 2 KiB erase page gives settings NVS four sectors; its default sector
count is capped by the actual partition size. RAK3172-F external flash
holds the secondary firmware image, not settings or modem context.

The AT sample enables RZI storage with settings NVS. The periodic-uplink
sample leaves RZI storage disabled and reads credentials from devicetree;
the modem still saves its own protocol context through its native HAL.
These are independent storage paths.

The RAK3172-F AT configuration enables `NVS_INIT_BAD_MEMORY_REGION`.
If NVS finds every sector closed and cannot recover the filesystem, it
erases the settings region and creates a new NVS. The RAK3172 AT
configuration leaves this option disabled and reports an initialization
error instead. This option is destructive recovery, not data migration.

The provided USP modem HAL updates context using read/erase/write of a
whole flash page. It has no redundant transactional copy, and the current
implementation ignores flash-operation return values. Abrupt power loss
during an update can therefore lose context; settings NVS tests do not
validate or repair this separate protocol-context path. The 8 KiB context
area has no room beyond the HAL's 8 KiB store-and-forward offset, so the
current board applications leave store-and-forward disabled.

`tests/storage/api` includes an NVS scenario using the same 8 KiB area,
2 KiB erase pages, and 8-byte write alignment. It checks value replacement,
exact-size reads, missing-key deletion, garbage collection, and retention
when rerun with the same simulator flash file. Hardware reset and power-cut
testing remain separate acceptance checks.

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
