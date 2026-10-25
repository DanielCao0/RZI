# Power API

Status: implemented

`include/rzi/power/power.h` provides service PM locks and helpers around
native Zephyr PM. RZI defines no sleep modes. Actual states use Zephyr's
`enum pm_state`; SoC support and state selection are described in
[power.md](./power.md).

The `rzi_power_*` interface is retained as the public API boundary for future
implementation changes. Its operations translate to native Zephyr APIs;
RZI-prefixed state names may be exposed when needed, with the same native
meanings. This interface does not add a separate state-selection policy.

Enable `CONFIG_RZI_POWER` for these helpers and `CONFIG_PM` separately for
system PM. Without system PM, ordinary CPU idle remains available; RZI
cannot disable it with a PM lock. Device runtime PM is independent.

## Functions

| Function | Role |
|---|---|
| `rzi_power_set_pm_enabled(bool)` | Acquire/release one native all-state PM lock |
| `rzi_power_get_pm_enabled()` | Read that runtime switch; false without `CONFIG_PM` |
| `rzi_power_lock_get()` / `rzi_power_lock_put()` | Take/release one named native all-state PM-lock reference |
| `rzi_power_get_lock_mask()` | Bitmask of owners with outstanding references |
| `rzi_power_sleep(timeout_ms)` | Block the calling thread through `k_sleep()` |

All APIs are for thread context. Error-returning APIs use negative
`RZI_ERR_*` values. State is statically initialized; repeated init calls
do not change hardware reset flags, settings, or lock references.

## System PM control

`rzi_power_set_pm_enabled(false)` acquires one
`pm_policy_state_all_lock_get()` reference. Passing `true` releases only
that reference. Repeated writes are idempotent. The initial switch permits
PM when `CONFIG_PM` is enabled; no separate RZI default-mode option exists.

The getter reports the switch, not effective state availability. PM locks,
other native locks, device readiness, kernel timeouts and residency times
still apply. A caller needing state-specific restrictions uses native
`pm_policy_state_lock_get(state, substate)` / `put()` directly.

The setter returns `-RZI_ERR_NOT_SUPPORTED` when `CONFIG_PM` is disabled.
`AT+LPM=0/1` uses the same setter; `AT+LPM=?` uses the getter. Query returns
0 without system PM, and attempts to change it return `AT_ERROR`.
Without the RZI power service, both AT reads and writes return `AT_ERROR`.

## PM locks

These locks are reference-counted power-state constraints, not mutual-exclusion
locks. Acquiring a reference does not wait for other owners to release theirs.

| PM lock | Owner |
|---|---|
| `APP` | Application |
| `LORAWAN` | Class C only |
| `FLASH` | Storage writes/deletes; FUOTA transfer/apply |
| `TRANSPORT` | Application during a transport session |
| `UPDATE` | Application during a dedicated update session |

Each successful take acquires one native all-state PM-lock reference and
requires a matching release. Releasing a missing
reference returns `-RZI_ERR_INVALID`; a full owner count returns
`-RZI_ERR_NO_RESOURCE`. Without `CONFIG_PM`, only reference counts are tracked.
`CONFIG_RZI_POWER_AUTO_SERVICE_LOCK=y` enables automatic service ownership;
Class A join/send does not take a PM lock for the whole radio exchange.

## Thread sleep

`rzi_power_sleep(int64_t timeout_ms)` waits for its relative timeout through
`k_sleep()`, or forever when negative. The native kernel timeout range
applies, and `k_wakeup()` may end the wait early. Use a kernel timer or
delayed work to schedule actions. Periodic delayed work does not need an
additional application sleep or RZI PM event.
