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

## Versioned customer records

The original raw read/write API remains unchanged. For evolving application
configuration, use `rzi_storage_write_record()` / `rzi_storage_read_record()`.
Records have a fixed 12-byte little-endian header: magic (4 bytes), application
schema version (2), payload length (2), CRC32 (4). CRC covers the first eight
header bytes followed by payload. Payload contents and migration rules belong
to the customer. This is corruption detection, not encryption or authentication.

A record is one settings value, so related fields are replaced together through
NVS. This does not make writes to several keys transactional. Reads validate
length and CRC before copying any outputs. Unsupported customer versions are
returned to the caller as version metadata; the caller decides whether to
migrate. The LoRaWAN schema rejects unknown policy versions explicitly.

For example, v1 stored a four-byte interval; v2 adds a four-byte threshold:

```c
#include <rzi/storage/storage.h>
#include <zephyr/sys/byteorder.h>

static int load_customer_config(uint32_t *interval, uint32_t *threshold)
{
    uint8_t data[8];
    uint16_t version;
    size_t size;
    int rc = rzi_storage_read_record("app", "config", &version,
                                     data, sizeof(data), &size);

    if (rc != 0) {
        return rc; /* Caller supplies defaults only for NOT_FOUND. */
    }
    if (version == 1 && size == 4) {
        sys_put_le32(80, data + 4);
        rc = rzi_storage_write_record("app", "config", 2, data, sizeof(data));
        if (rc != 0) {
            return rc; /* Old record remains authoritative if saving failed. */
        }
    } else if (version != 2 || size != sizeof(data)) {
        return -RZI_ERR_NOT_SUPPORTED; /* Preserve unknown future schemas. */
    }
    *interval = sys_get_le32(data);
    *threshold = sys_get_le32(data + 4);
    return 0;
}
```

Encode fields explicitly rather than writing C struct padding or pointers.
`CONFIG_RZI_STORAGE_RECORD_MAX_SIZE` limits payload and scratch-buffer size
(default 256 bytes). Each read/write uses a scratch buffer of that size plus
12 bytes on the calling thread's stack. Backend per-value limits also apply.
Existing raw values do not become versioned automatically: migrate deliberately
or choose a new key. Record and raw APIs share the same namespace and key space.

## Capacity accounting and budgets

`rzi_storage_get_info()` reports NVS-managed bytes, the backend free-space
estimate, sector size and contiguous space in the current sector. Other settings
backends return NOT_SUPPORTED. The snapshot does not reserve space; concurrent
writers and settings metadata can reduce what a subsequent write can store.
`rzi_storage_get_usage("app", &usage)` counts live keys, relative key-name bytes,
and stored value bytes within the namespace. These logical totals exclude NVS
alignment and metadata. Queries read storage and should be diagnostic operations,
not part of a high-frequency sampling loop.

For the 8 KiB / four-sector STM32WLE5 layout, NVS reserves a sector for garbage
collection. Do not budget all 8192 bytes as customer payload. A conservative
initial engineering budget is at most 2 KiB of live customer values, leaving
room for framework keys, names, records and garbage collection; validate the
actual key count and churn with capacity queries and ENOSPC tests. This budget
is guidance, not an enforced quota or a guaranteed allocation. Writes that run
out of space return `-RZI_ERR_NO_RESOURCE`; callers must handle it.

Namespaces prevent key collisions but share physical capacity. This change
preserves existing board partitions and user data. Products requiring guaranteed
independent customer capacity must reserve a separate partition/backend before
shipping; a namespace alone cannot provide that guarantee. Large telemetry
queues and high-frequency samples should not use this small settings partition.

## Security

The storage service does not encrypt secret values. Protocol-stack context
storage is managed by the selected backend independently of settings.
Version metadata and CRC support customer schema migration; updates to
separate keys are not a transaction.
