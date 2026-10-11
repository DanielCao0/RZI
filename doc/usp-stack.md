# USP stack

Status: implemented

How RZI sits on top of Semtech USP (`modules/lib/usp`) and its Zephyr port
(`modules/usp_zephyr`), with three call chains traced through real functions.

See also: [architecture.md](./architecture.md),
[lorawan-backends.md](./lorawan-backends.md),
[lora-api.md](./lora-api.md),
[at-framework.md](./at-framework.md).

## Repositories

| Repository | Path | Role | Zephyr-specific |
|---|---|---|---|
| `rzi` | `rzi/` | Product application: AT commands, services, backends | Yes |
| `usp_zephyr` | `modules/usp_zephyr` | Zephyr module: HAL implementation, radio BSP, USP thread, Kconfig/CMake glue, boards, samples | Yes |
| `usp` | `modules/lib/usp` | Portable core: RAC, radio planner, RAL/RALF, chip drivers, protocols | No |

LoRa Basics Modem (LBM) is not a separate repository. It is the protocol
library at `modules/lib/usp/protocols/lbm_lib`, and it runs on top of RAC.

`usp_zephyr/west.yml` pins `usp` (currently `v1.2.1`) at `modules/lib/usp`.
`usp` has no Zephyr build logic of its own:
`usp_zephyr/modules/usp/CMakeLists.txt` sets `RAC_LIB_DIR` and `LBM_LIB_DIR`
and picks the sources to compile from Kconfig. LBM is compiled only with
`CONFIG_USP_LORA_BASICS_MODEM=y`.

`usp` declares the HAL interface (`smtc_modem_hal.h`).
`usp_zephyr/modules/smtc_modem_hal/` implements it. Porting to another OS
means rewriting only that layer and the radio BSP.

## Layers

```mermaid
flowchart TB
    subgraph RZI["rzi (rzi/src)"]
        direction TB
        AT["at/<br/>at_lorawan_join_send.c (AT+SEND, AT+JOIN)<br/>at_lora_p2p.c (AT+PSEND)"]
        SVC["Services (backend-agnostic)<br/>lorawan/lorawan.c: rzi_lorawan_send()<br/>lora/lora.c: rzi_lora_send()"]
        BE["Backends (vtable)<br/>lorawan/backend/usp/*.c: usp_send()<br/>lora/backend/lora_backend_usp.c: submit()"]
        PLAT["usp/usp_platform.c<br/>rzi_usp_init / rzi_usp_lock / rzi_usp_unlock<br/>modem_event_callback"]
        AT --> SVC --> BE
        BE --> PLAT
    end

    subgraph USP["usp (modules/lib/usp), OS-independent"]
        direction TB
        LBM["protocols/lbm_lib (LBM)<br/>smtc_modem_* → modem_supervisor<br/>→ lorawan_api → lr1mac"]
        RAC["smtc_rac_lib/smtc_rac<br/>smtc_rac_* API"]
        RP["smtc_rac_lib/radio_planner<br/>rp_task_enqueue, arbitration, IRQ dispatch"]
        RAL["smtc_ralf → smtc_ral<br/>radio abstraction"]
        DRV["radio_drivers/lr20xx_driver<br/>lr20xx_radio_* (builds SPI commands)"]
        HALIF["smtc_modem_hal.h<br/>(interface only)"]
        LBM --> RP
        RAC --> RP
        RP --> RAL --> DRV
    end

    subgraph UZ["usp_zephyr (modules/usp_zephyr), Zephyr port"]
        direction TB
        THR["subsys/usp/zephyr_usp_thread.c<br/>USP thread: smtc_modem_run_engine()<br/>+ smtc_rac_run_engine()"]
        HAL["modules/smtc_modem_hal/smtc_modem_hal.c<br/>timers, storage, IRQ registration"]
        BSP["drivers/usp/lr20xx/<br/>lr20xx_hal.c (SPI, BUSY pin)<br/>lr20xx_board.c (DIO interrupts)"]
    end

    Z["Zephyr: spi_transceive_dt, gpio, k_timer, settings"]
    CHIP(["LR2021 radio"])

    BE -- "LoRaWAN: smtc_modem_request_uplink()" --> LBM
    BE -- "P2P: smtc_rac_submit_radio_transaction()" --> RAC
    PLAT -- "smtc_rac_init(), smtc_modem_init()" --> USP
    PLAT -. "shared rac_api_mutex" .- THR
    THR --> LBM
    THR --> RAC
    HAL -. "implements" .-> HALIF
    DRV -- "lr20xx_hal_write()" --> BSP
    BSP --> Z --> CHIP
    HAL --> Z
```

LoRaWAN goes through LBM. Raw LoRa / FSK (P2P) skips LBM and submits
transactions straight to RAC. LBM and the RZI P2P backend are therefore peer
RAC users that queue in the same radio planner.

| Layer | Repository | Example functions | Owns |
|---|---|---|---|
| AT commands | rzi | `handle_psend()` | Parsing, `OK` / `ERROR` replies |
| Services | rzi | `rzi_lorawan_send()`, `rzi_lora_send()` | Validation and state; backend-agnostic (USP, Zephyr, test) |
| Backends | rzi | `usp_send()`, `submit()` | Mapping RZI semantics to Semtech APIs, locking |
| Platform | rzi | `rzi_usp_lock()`, `modem_event_callback()` | USP init, mutual exclusion with the USP thread, LBM event dispatch |
| LBM | usp | `smtc_modem_request_uplink()` → `lr1mac_core_payload_send()` | LoRaWAN MAC |
| RAC and radio planner | usp | `smtc_rac_submit_radio_transaction()`, `rp_task_enqueue()` | Multi-protocol arbitration and scheduling |
| RAL and drivers | usp | `ralf_setup_lora()` → `lr20xx_radio_lora_set_modulation_params()` | Radio parameters to chip commands |
| Zephyr port | usp_zephyr | `lr20xx_hal_write()`, USP thread, `smtc_modem_hal_*` | SPI, GPIO, timers, thread, HAL implementation |
| OS | zephyr | `spi_transceive_dt()` | Peripheral access |

## Example 1: `AT+SEND` (LoRaWAN uplink)

The AT thread only queues the uplink. The USP thread drives the radio.

```mermaid
sequenceDiagram
    autonumber
    participant AT as RZI AT thread<br/>at_lorawan_join_send.c
    participant SVC as rzi_lorawan_send()<br/>lorawan.c
    participant BE as usp_send()<br/>lorawan_backend_usp.c
    participant LBM as LBM<br/>smtc_modem.c
    participant T as USP thread<br/>zephyr_usp_thread.c
    participant MAC as modem_tx_protocol_manager<br/>+ lr1mac
    participant RP as radio_planner
    participant DRV as ralf / ral / lr20xx_driver
    participant BSP as lr20xx_hal.c (usp_zephyr)
    participant Z as Zephyr SPI

    AT->>SVC: rzi_lorawan_send(port, data, type)
    SVC->>SVC: validate, check_joined()
    SVC->>BE: rzi_lorawan_backend.send()
    BE->>BE: rzi_lorawan_usp_enter() → rzi_usp_lock()
    BE->>LBM: smtc_modem_request_uplink()
    LBM->>LBM: smtc_modem_send_tx() → lorawan_send_add_task()
    LBM-->>BE: SMTC_MODEM_RC_OK (queued only)
    BE->>BE: tx_pending = true<br/>rzi_lorawan_usp_finish() → unlock + smtc_modem_hal_wake_up()
    BE-->>AT: 0 → "OK"

    Note over T: woken by smtc_modem_hal_wake_up()
    T->>LBM: smtc_modem_run_engine()
    LBM->>MAC: lorawan_api_payload_send() → lr1mac_core_payload_send()
    MAC->>RP: rp_task_enqueue(tx_payload, radio_params)
    T->>RP: smtc_rac_run_engine(): arbitration, task is due
    RP->>MAC: lr1_stack_mac_tx_lora_launch_callback_for_rp()
    MAC->>DRV: ralf_setup_lora() → ral_lr20xx_set_lora_mod_params()<br/>→ lr20xx_radio_lora_set_modulation_params()
    DRV->>BSP: lr20xx_hal_write(cmd)
    BSP->>BSP: wait for BUSY low
    BSP->>Z: spi_transceive_dt()
```

## Example 2: `AT+PSEND` (raw LoRa, no LBM)

```mermaid
sequenceDiagram
    autonumber
    participant AT as at_lora_p2p.c<br/>handle_psend()
    participant SVC as rzi_lora_send()<br/>lora.c
    participant BE as lora_backend_usp.c
    participant RAC as smtc_rac.c / smtc_rac_lora.c
    participant RP as radio_planner
    participant DRV as ral → lr20xx_driver → lr20xx_hal.c

    AT->>SVC: rzi_lora_send(payload)
    SVC->>SVC: apply_cipher() (optional XOR)
    SVC->>BE: rzi_lora_backend.send()
    BE->>BE: submit(TX_ID): fill_lora() into smtc_rac_context_t<br/>SF, BW, CR, frequency, power, SMTC_RAC_ASAP_TRANSACTION
    BE->>RAC: smtc_rac_submit_radio_transaction(TX_ID)
    RAC->>RP: smtc_rac_lora() → rp_task_enqueue()
    Note over RP: USP thread runs smtc_rac_run_engine()<br/>LoRaWAN traffic may queue ahead or preempt by priority
    RP->>DRV: configure modulation, transmit over SPI
```

Because both paths share the radio planner, `on_tx_done()` in
`lora_backend_usp.c` maps `RP_STATUS_TASK_ABORTED` (preempted by LoRaWAN or
cancelled by `stop()`) to `-RZI_ERR_BUSY`.

## Example 3: TX-done interrupt back to RZI

```mermaid
sequenceDiagram
    autonumber
    participant CHIP as LR2021
    participant BSP as lr20xx_board.c<br/>(usp_zephyr)
    participant HAL as smtc_modem_hal.c<br/>(usp_zephyr)
    participant RP as radio_planner<br/>(usp)
    participant T as USP thread
    participant LBM as LBM (lr1mac, supervisor)
    participant PLAT as usp_platform.c (rzi)
    participant BE as lorawan_backend_usp.c (rzi)
    participant AT as AT output (URC)

    CHIP->>BSP: DIO rising edge (TX_DONE)
    BSP->>BSP: lr20xx_board_event_callback()
    BSP->>HAL: event_interrupt_cb()
    HAL->>RP: rp_radio_irq_callback()<br/>(registered by smtc_rac_init() via smtc_modem_hal_irq_config_radio_irq())
    RP->>RP: set radio_irq_flag, wake USP thread
    T->>RP: smtc_rac_run_engine() handles the IRQ
    RP->>LBM: task-done callback → open RX1 / RX2 windows
    T->>LBM: smtc_modem_run_engine() raises SMTC_MODEM_EVENT_TXDONE
    LBM->>PLAT: modem_event_callback()
    loop while pending > 0
        PLAT->>LBM: smtc_modem_get_event()
        PLAT->>BE: handler → handle_event(event)
    end
    BE->>BE: case SMTC_MODEM_EVENT_TXDONE: tx_pending = false
    BE->>AT: report to the LoRaWAN service → asynchronous AT event
```

## Locking

Every `smtc_*` call from RZI must sit between `rzi_usp_lock()` and
`rzi_usp_unlock()`. The lock is `rac_api_mutex`, the same mutex the USP thread
holds around `smtc_modem_run_engine()` and `smtc_rac_run_engine()`, so
`usp_platform.c` enforces `BUILD_ASSERT(CONFIG_USP_THREADS_MUTEXES)`.
`rzi_usp_unlock()` also calls `smtc_modem_hal_wake_up()` so the USP thread
processes newly queued work immediately instead of waiting for its next
timeout.

`modem_event_callback()` and the handler installed with
`rzi_usp_set_event_handler()` run in the USP thread with the lock held. They
must not block on anything that waits for the USP thread.

## Relation to the usp_zephyr samples

`samples/usp/` in `usp_zephyr` is split by the layer each sample exercises:

| Directory | Uses | RZI equivalent |
|---|---|---|
| `lbm/` (for example `periodical_uplink`) | LBM `smtc_modem_*`; RAC runs underneath | `lorawan/backend/usp` |
| `rac/` (for example `multiprotocol`) | LBM and another protocol sharing the radio through RAC | LoRaWAN and P2P together |
| `sdk/` (for example `ping_pong`) | Raw modulations submitted to RAC, no LoRaWAN | `lora/backend/lora_backend_usp.c` |
