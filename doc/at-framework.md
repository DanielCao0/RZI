# RZI AT Framework

The RZI AT framework provides RUI3-compatible command behavior without tying
commands to a serial driver or a protocol backend.

## Boundaries

The core owns line framing, RUI3 grammar, command dispatch, responses,
unsolicited events, and one execution thread. A transport only moves bytes. A
command package translates requests into public RZI service calls. It never
calls a private LoRaWAN backend or owns a hardware transport.

```text
UART / future BLE / test transport
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

- `CONFIG_RZI_AT_TRANSPORT_UART`: interrupt-driven UART adapter;
- `CONFIG_RZI_AT_COMMAND_LORAWAN`: RUI3 LoRaWAN command package;
- `CONFIG_RZI_AT_NVM`: persistence for the current LoRaWAN command settings;
- `CONFIG_RZI_AT_ECHO`: terminal input echo.

Buffer sizes, registry capacity, thread stack, priority, and extension polling
interval are separate Kconfig settings. Future command packages and transports
must follow the same dependency direction.

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

The selected transport instance remains owned by the application. The UART
adapter takes exclusive use of the selected device after successful start; the
same UART must not also be used by the Zephyr console, shell, or logging backend.

The initial implementation is process-wide and supports one active transport.
Adding multiple simultaneous ports requires a per-session parser and response
context; it must not introduce global response routing into command handlers.
