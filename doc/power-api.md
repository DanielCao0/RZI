# Power API

Status: implemented

C contract for product sleep policy. The header is
`include/rzi/power/power.h`. Sleep depth on each board is in
[power.md](./power.md).

See also: [power.md](./power.md), [rsup-api.md](./rsup-api.md).

RZI coordinates **policy** and **blockers** only. Zephyr performs the CPU /
peripheral transition. LoRaWAN RX1/RX2 timing belongs to the backend. This
service does not compute or delay receive windows.

`CONFIG_RZI_POWER=y` enables the service. The default also enables
`CONFIG_RZI_POWER_AUTO_SERVICE_BLOCK`, so LoRaWAN Class C, storage, RSUP,
and FUOTA take blockers automatically. Class A join/send do not.

## Boards

Sleep depth on each product board is in [power.md](./power.md). 4631 stays
System ON + WFE and does not enable `CONFIG_PM`. 3372 may enter STOP2.

## Policy and blockers

| Policy | Behavior |
|---|---|
| `RUN` | Lock every Zephyr PM state |
| `IDLE` | CPU idle only. Everyday 4631 path |
| `SUSPEND` | Deepest RAM-retaining sleep (STOP2 on 3372) |
| `SHUTDOWN` | Idle does not power off; call `rzi_power_shutdown()` |

| Blocker | Who takes it |
|---|---|
| `APP` | Application |
| `LORAWAN` | Class C only |
| `FLASH` | Storage write/delete; FUOTA transfer or apply |
| `TRANSPORT` | Application during a USB/UART session |
| `UPDATE` | `rzi_rsup_run()` |

The same blocker is reentrant. `rzi_power_is_sleep_allowed()` is true when
the policy is idle or suspend and every count is zero.

## Functions

| Function | Role |
|---|---|
| `rzi_power_init()` | Idempotent; captures reset cause |
| `rzi_power_set_policy()` / `rzi_power_get_policy()` | Product sleep depth |
| `rzi_power_block()` / `rzi_power_unblock()` | Take or release one reference |
| `rzi_power_get_blockers()` | Bitmask of non-zero blockers |
| `rzi_power_is_sleep_allowed()` | Idle/suspend and no blockers |
| `rzi_power_set_wake_deadline()` / `rzi_power_get_wake_deadline()` | Next known work time, not an RX window |
| `rzi_power_set_wake_sources()` / `rzi_power_get_wake_sources()` | Advisory mask checked at shutdown |
| `rzi_power_get_wake_reason()` | Snapshot from `rzi_power_init()` |
| `rzi_power_sleep(timeout_ms)` | Suspend the caller; never shutdown |
| `rzi_power_shutdown()` | RAM-lost power-off; does not return on success |

Wake-source flags: `RZI_POWER_WAKE_TIMER`, `_GPIO`, `_UART`, `_USB`,
`_RADIO`. On 4631, TIMER alone cannot wake from System OFF.

## Application usage

```c
(void)rzi_power_init();
(void)rzi_power_set_policy(RZI_POWER_POLICY_SUSPEND); /* RUI3 lpm.set(1) */

(void)rzi_power_block(RZI_POWER_BLOCK_TRANSPORT);
/* wait for USB DTR */
(void)rzi_power_unblock(RZI_POWER_BLOCK_TRANSPORT);
```

Periodic uplinks come from a timer or a LoRaWAN callback calling
`rzi_lorawan_send()`. The main thread uses `k_sleep(K_FOREVER)`. Do not
`rzi_power_sleep(fixed duration)` over an in-flight Class A session.

`rzi_power_shutdown()` returns `-RZI_ERR_BUSY` / `-RZI_ERR_NOT_SUPPORTED` when the policy is not
shutdown, blockers remain, or the wake-source mask cannot return from
power-off.

Acceptance still measures idle / joined current on hardware. This API only
guarantees the policy and blocker contract.
