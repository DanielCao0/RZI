.. zephyr:code-sample:: rzi_at
   :name: RZI AT
   :relevant-api: rzi_at rzi_lorawan rzi_lora

   RAK RUI3-compatible AT command interface over USB CDC UART.

Overview
********

This sample starts the RZI AT command service on the USB CDC serial port.
The UART adapter feeds the I/O-independent AT framework. The command
packages follow the command behaviors, status strings and asynchronous
events of the RAK RUI3 AT Command Manual, so host software written for RUI3
modules can drive it directly.

The sample enables both command packages:

``CONFIG_RZI_AT_COMMAND_LORAWAN``
   LoRaWAN commands on the RZI LoRaWAN C API: keys, ``AT+BAND``,
   ``AT+JOIN``, ``AT+SEND``, ``AT+RECV``, ADR, data rate, channel masks,
   multicast and more.

``CONFIG_RZI_AT_COMMAND_LORA``
   P2P commands on the RZI raw LoRa API (``AT+P2P``, ``AT+PSEND``,
   ``AT+PRECV`` and the ``AT+P*`` parameters) and radio tests (``AT+TTX``,
   ``AT+TRX``, ``AT+TRSSI``, ``AT+CW`` and others).

``AT+NWM`` switches between them: ``0`` is P2P LoRa, ``1`` is LoRaWAN (the
default) and ``2`` is P2P FSK. The mode is stored in flash and the board
reboots into it. P2P and radio-test commands return ``AT_BUSY_ERROR`` in
LoRaWAN mode.

Remove either package from ``prj.conf`` for a LoRaWAN-only or P2P-only
modem; ``main.c`` does not change.

LoRaWAN events: ``+EVT:JOINED``, ``+EVT:JOIN_FAILED_RX_TIMEOUT``,
``+EVT:TX_DONE``, ``+EVT:SEND_CONFIRMED_OK``, ``+EVT:SEND_CONFIRMED_FAILED``
and ``+EVT:RX_1:<rssi>:<snr>:UNICAST:<port>:<payload>``. P2P events:
``+EVT:TXP2P DONE`` and ``+EVT:RXP2P:<rssi>:<snr>:<hex>``.

Persistence
***********

With ``CONFIG_RZI_AT_NVM`` (enabled in this sample) the parameters set over
AT — network mode, DevEUI, JoinEUI, AppKey, band, confirm mode and auto-join
settings — are
stored in flash through the Zephyr settings subsystem (NVS backend) and survive
reboots. ``ATR`` erases them and reboots, restoring the devicetree defaults.

RZI does not hardcode any flash address. The storage area is board-defined
through devicetree, following the standard Zephyr convention:

* a partition labeled ``storage_partition`` in the RZI product-board
  partition table (``rak4631/nrf52840/rzi`` places 32 KiB at the end of internal flash), or
* a ``zephyr,settings-partition`` chosen node pointing at any partition,
  including one on external SPI flash.

Boards without either definition fail at build time instead of writing to a
wrong address at runtime. ``AT+JOIN`` supports the RUI3 start, auto-join,
retry-interval and retry-count parameters. ABP remains a planned service.

See ``doc/at-command-compatibility.md`` for the complete command reference and
the precise compatibility limits compared with RUI3.

Requirements
************

* An RZI RAK4631 (``rak4631/nrf52840/rzi``) with its LoRa antenna connected
* For LoRaWAN mode: a gateway, a network server and valid OTAA identifiers
  and keys
* For P2P mode: a second board running this sample or ``samples/lora/ping_pong``

Set keys at runtime with ``AT+DEVEUI``, ``AT+APPEUI`` and ``AT+APPKEY``.
Hardware and the AT UART alias live on the product board.

AT UART selection
*****************

The sample selects its exclusive AT UART adapter with the ``rzi-at-uart``
devicetree alias. It does not reuse ``zephyr,console`` because console and AT
adapter ownership are separate concerns. ``rak4631/nrf52840/rzi`` points the alias to
USB CDC ACM. ``rak3362`` has no USB controller and points it to ``uart20``,
which the base board USB-C port reaches through a USB-UART converter.
``rak3172/stm32wle5xx/rzi`` points it to ``lpuart1`` (PA2/PA3), matching the RUI3 AT
port on RAK3172. Other samples use that same LPUART as ``zephyr,console``.
UART1 remains ``usart1`` (PB6/PB7):

.. code-block:: devicetree

   aliases {
       rzi-at-uart = &board_cdc_acm_uart;
   };

An application can point the same alias to a ready physical UART instead.
Logging and console output must remain disabled on a UART owned by the AT
service.

Building
********

From the workspace root:

.. code-block:: console

   west build -p always -b rak4631/nrf52840/rzi --sysbuild rzi/samples/at

Flash ``build/merged_rak4631_nrf52840_rzi.hex``. See ``doc/boot.md``.

For RAK3362, use ``-b rak3362/nrf54l15/cpuapp``.

MCUboot logging and the boot banner are off (``sysbuild/mcuboot.conf``), so
on RAK3362, where MCUboot shares uart20 with the AT port, a host sees no boot
log. Once the AT service is ready, the sample writes one line with the same
value as ``AT+VER``:

.. code-block:: text

   +EVT:BOOT:RZI_0.2.0_rak3362

For RAK3372, use ``-b rak3172/stm32wle5xx/rzi``. The AT port is lpuart1
(PA2/PA3). This sample turns the console off.

Example session
***************

Open the USB CDC port (RAK4631), the base board serial port (RAK3362), or
LPUART1 (PA2/PA3) on RAK3372 at 115200 baud. On RAK4631 the sample waits up to 10 s for the host to open the
port, so the boot line is only seen if the port opens within that time. Then
type:

.. code-block:: text

   AT
   OK
   AT+VER=?
   AT+VER=RZI_0.2.0_rak4631
   OK
   AT+DEVEUI=0011223344556677
   OK
   AT+APPEUI=0102030405060708
   OK
   AT+APPKEY=01020AFBA1CD4D20010230405A6B7F88
   OK
   AT+BAND=4
   OK
   AT+JOIN
   OK
   +EVT:JOINED
   AT+SEND=2:112233
   OK
   +EVT:TX_DONE

To switch to P2P LoRa, set the mode and reconnect after the reboot.
``AT+PSEND`` then transmits with the current ``AT+P2P`` settings:

.. code-block:: text

   AT+NWM=0
   OK
   AT+PSEND=112233
   OK
   +EVT:TXP2P DONE

Troubleshooting
***************

No USB serial port appears
   The board was flashed with ``zephyr.hex`` instead of the merged image, or
   the USB cable is power-only.

No response to ``AT``
   Check that the terminal sends a line ending (CR, LF or CR LF). Logging and the
   console are disabled in this sample, so the port carries AT traffic only.

``+EVT:JOIN_FAILED_RX_TIMEOUT``
   The network server did not answer. Check the keys, ``AT+BAND`` and gateway
   coverage; see the troubleshooting section of ``samples/lorawan/periodical_uplink``.
