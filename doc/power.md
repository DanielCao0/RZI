# Power management

Status: implemented

RZI uses Zephyr's system PM states, policy, and reference-counted locks.
There is no RZI sleep-mode enum or second state-selection policy.
See [power-api.md](./power-api.md) for the C API.

## RZI API boundary

Keep `rzi_power_*` as the application-facing API and translate its operations
to Zephyr in `power.c`. This boundary leaves room for future implementation
changes without introducing another power-management model.

- Reuse native state meanings and lock/event semantics. RZI-prefixed state
  names, when needed by an API, must map one-to-one to native states.
- Let Zephyr select states and perform transitions. Keep platform wake and
  peripheral configuration in the board and drivers.
- Retain only the translation and reference ownership required by existing
  operations. Extend the interface when a concrete caller needs it.

## Native states and ownership

Zephyr defines `enum pm_state` in `<zephyr/pm/state.h>`:

| Native state | Meaning |
|---|---|
| `PM_STATE_ACTIVE` | Active execution |
| `PM_STATE_RUNTIME_IDLE` | Runtime idle; devices retain their existing states |
| `PM_STATE_SUSPEND_TO_IDLE` | Suspend without losing CPU execution context |
| `PM_STATE_STANDBY` | Platform standby |
| `PM_STATE_SUSPEND_TO_RAM` | Suspend with context retained in RAM |
| `PM_STATE_SUSPEND_TO_DISK` | Save context to nonvolatile storage before suspend |
| `PM_STATE_SOFT_OFF` | Power off without preserving execution context |

These names describe Zephyr states. Their availability and hardware mapping
come from each SoC's device tree and PM driver. They are not interchangeable
across boards. For example, STM32WL maps STOP0/1/2 to
`PM_STATE_SUSPEND_TO_IDLE` with different substate IDs.

The application enables `CONFIG_PM`. The idle thread and Zephyr PM policy
then select a state using configured `cpu-power-states`, residency times,
exit latency, pending events, and native locks. RZI does not select a
specific state, force STOP2, or promise RAM retention for arbitrary boards.

| Layer | Responsibility |
|---|---|
| Application / board | `CONFIG_PM`, device tree, wake peripherals, native state constraints |
| Zephyr | State selection, idle entry/resume, device PM |
| RZI power | Runtime PM-lock switch, named service references |
| LoRaWAN backend | TX/RX1/RX2 timing, radio IRQs, stack timers |

`CONFIG_RZI_POWER` enables the helpers without selecting `CONFIG_PM`.
Ordinary CPU idle still works when system PM is disabled. Device runtime PM
is independent of the system PM switch.

## Runtime PM switch

`rzi_power_set_pm_enabled(false)` takes one native
`pm_policy_state_all_lock_get()` reference. Passing `true` releases only
that reference. Repeated calls are idempotent; enabling does not release
PM locks or locks owned by other code.

`rzi_power_get_pm_enabled()` reads this same switch for both API and AT
clients. It returns false if `CONFIG_PM` is disabled. Changing the switch
without system PM returns `-RZI_ERR_NOT_SUPPORTED`.

`AT+LPM=0` acquires the switch's lock; `AT+LPM=1` releases it. `AT+LPM=?`
reads the switch. A value of 1 means this switch permits system PM, not
that a specific state is reachable or that every native lock is released.
A system PM lock does not prevent ordinary CPU idle or control device
runtime PM.

Applications that need state-specific restrictions use Zephyr directly:

```c
#include <zephyr/pm/policy.h>

/* Prevent this native state and all its substates during an operation. */
pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
/* Perform the operation. */
pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
```

The native lock API only affects states present in the device tree; it
neither adds a state nor enables an unsupported hardware mode.

## Service PM locks

Each successful `rzi_power_lock_get()` takes one native all-state PM lock.
Each matching `rzi_power_lock_put()` releases one. RZI tracks counts by
owner so a mismatched release fails instead of unlocking somebody else's
reference.
Storage aborts before a write/delete when its reference cannot be acquired.
Class C state and reference ownership are updated under the same config
mutex, including releases on leave.
Without `CONFIG_PM`, only reference counts are tracked; these references
do not restrict ordinary CPU idle or device runtime PM.

With `CONFIG_RZI_POWER_AUTO_SERVICE_LOCK=y`:

| PM lock | Owner |
|---|---|
| `LORAWAN` | Class C; released when leaving Class C or leaving the network |
| `FLASH` | Settings writes/deletes and FUOTA transfer/apply |
| `APP`, `TRANSPORT`, `UPDATE` | Application-managed, paired references |

Class A join/send does not hold a PM lock for the entire exchange. The
backend schedules TX and receive windows so the CPU may sleep between
radio events. RZI does not calculate or delay RX1/RX2.

## Thread waits

`rzi_power_sleep(int64_t timeout_ms)` is a thin `k_sleep()` wrapper. A
negative value waits forever, and `k_wakeup()` may end the wait early.
Native timeout-range limits apply, without an additional 32-bit millisecond
cap. System sleep follows when the scheduler has no runnable work and the
native PM policy permits it.

Use a kernel timer or delayed work to run an action at a given time.
Periodic uplinks already scheduled by delayed work need no additional
application sleep or RZI PM event.

## Current periodic-uplink boards

- RAK3172 and RAK3172-F enable `CONFIG_PM` in their sample board configs.
  The sample uses SysTick with an RTC companion; Zephyr selects among the
  STM32WL STOP substates when possible.
- The current nRF52840 and RAK3362/nRF54L15 configurations leave system PM
  disabled. Their ordinary CPU idle path continues to work; the runtime
  system PM switch cannot disable that path. RAK3362 separately enables
  device runtime PM for its configured peripherals.

A new ESP or nRF target uses its native PM driver and device tree. It does
not need a new RZI mode or a platform-name switch in the state-selection
code. Physical current and radio timing still require board testing.

All persistent coordinator state is statically initialized; the power APIs
require no explicit initialization.

The implementation is in `src/power/power.c`; API and PM-lock tests are in
`tests/power/api`. AT integration tests are in `tests/at/framework`.
