# RZI AT Framework

The RZI AT framework provides RUI3-compatible command behavior without tying
commands to a serial driver or a protocol backend.

## Boundaries

The core owns line framing, RUI3 grammar, command dispatch, responses,
unsolicited events, and one execution thread. An adapter only moves bytes
between a specific I/O API and the core. A command package translates requests
into public RZI service calls. It never calls a private LoRaWAN backend or owns
a hardware adapter.

```text
UART / future RUI3 BLE UART adapter
                |
                v
       RX ring and AT thread
                |
                v
       parser -> registry
                    |
          +---------+----------+
          |                    |
     system commands     LoRaWAN commands
                               |
                               v
                    public RZI LoRaWAN API
```

## Configuration

`CONFIG_RZI_AT` enables only the core framework. It does not enable LoRaWAN or
UART. The current optional pieces are:

- `CONFIG_RZI_AT_ADAPTER_UART`: interrupt-driven UART I/O adapter;
- `CONFIG_RZI_AT_COMMAND_LORAWAN`: RUI3 LoRaWAN command package;
- `CONFIG_RZI_AT_COMMAND_LORA`: reserved raw LoRa and FSK command package;
- `CONFIG_RZI_AT_NVM`: persistence through the public RZI storage service;
- `CONFIG_RZI_AT_ECHO`: terminal input echo.

Buffer sizes, registry capacity, thread stack, priority, and extension polling
interval are separate Kconfig settings. Future command packages and I/O
adapters must follow the same dependency direction. The BLE UART adapter is a
reserved `0.0.0` boundary based on RUI3 `SERIAL_BLE0` and is not compiled.
Zephyr USB CDC ACM uses the implemented UART adapter.

The authoritative list of implemented commands and the compatibility limits of
each command are documented in
[`at-command-compatibility.md`](./at-command-compatibility.md).

## Command package organization

Small packages may use one source file. Larger packages use a package
subdirectory and split handlers by stable command domain. The LoRaWAN package
follows the RUI3 domains where they are useful:

- `key_id`: identifiers and root keys;
- `join_send`: activation, uplink, and downlink;
- `network_management`: network mode, region, and device class.

Each domain owns native `struct rzi_at_command` descriptors. The descriptor is
the single source of truth for the command name, help, allowed operations, and
handler. RZI does not copy RUI3's separate `_def.h` pattern because that would
duplicate metadata already represented by the registry.

## Extending commands

An application declares one or more static-lifetime `struct rzi_at_command`
objects and registers them before starting the AT service. Each entry declares
its command name, help text, allowed RUI3 operations, handler, and optional
context. Registration validates the complete batch, rejects duplicate or
invalid names, and becomes immutable when the service starts.

Handlers use `rzi_at_respond_status()` or `rzi_at_respond_value()`. Components
publish asynchronous RUI3 events with `rzi_at_publish_event()`. The core
serializes all output, including events produced by other threads.

## Execution and ownership

`rzi_at_receive()` is ISR-safe. It copies bytes into a fixed ring buffer and
wakes the AT thread. Parsing and command handlers never run in interrupt
context. An RX overflow resets the partial command before reporting an overflow
response, so truncated input cannot be executed.

The selected I/O binding remains owned by the application. The UART
adapter takes exclusive use of the selected device after successful start; the
same UART must not also be used by the Zephyr console, shell, or logging backend.

The initial implementation is process-wide and supports one active I/O adapter.
Adding multiple simultaneous ports requires a per-session parser and response
context; it must not introduce global response routing into command handlers.
