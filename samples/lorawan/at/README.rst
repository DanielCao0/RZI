.. zephyr:code-sample:: rzi_lorawan_at
   :name: RZI LoRaWAN AT
   :relevant-api: rzi_lorawan

   RAK RUI3-compatible AT command interface over USB CDC UART.

Overview
********

This sample starts the RZI AT command service on the USB CDC serial port.
The UART adapter feeds the I/O-independent AT core. The optional
LoRaWAN command package consumes the RZI LoRaWAN C API and follows the command
behaviors, status strings and asynchronous events of the RAK RUI3 AT Command
Manual, so host software written for RUI3 modules can drive it directly.

The relevant configuration switches are ``CONFIG_RZI_AT``,
``CONFIG_RZI_AT_ADAPTER_UART`` and ``CONFIG_RZI_AT_COMMAND_LORAWAN``.

Implemented subset (OTAA, Class A):

* ``AT``, ``ATZ``, ``ATR``, ``AT+VER``
* ``AT+DEVEUI``, ``AT+APPEUI``, ``AT+APPKEY``
* ``AT+BAND`` (EU433 and LA915 are not supported by the backend)
* ``AT+NWM`` (LoRaWAN mode only), ``AT+NJM`` (OTAA only), ``AT+NJS``,
  ``AT+CLASS`` (Class A only)
* ``AT+CFM``, ``AT+CFS``
* ``AT+JOIN``, ``AT+SEND``, ``AT+RECV``

Asynchronous events: ``+EVT:JOINED``, ``+EVT:JOIN_FAILED_RX_TIMEOUT``,
``+EVT:TX_DONE``, ``+EVT:SEND_CONFIRMED_OK``, ``+EVT:SEND_CONFIRMED_FAILED``
and ``+EVT:RX_1:<rssi>:<snr>:UNICAST:<port>:<payload>``.

Persistence
***********

With ``CONFIG_RZI_AT_NVM`` (enabled in this sample) the parameters set over
AT — DevEUI, JoinEUI, AppKey, band, confirm mode and auto-join settings — are
stored in flash through the Zephyr settings subsystem (NVS backend) and survive
reboots. ``ATR`` erases them and reboots, restoring the devicetree defaults.

RZI does not hardcode any flash address. The storage area is board-defined
through devicetree, following the standard Zephyr convention:

* a partition labeled ``storage_partition`` in the RZI product-board
  partition table (``rzi_rak4631`` places 32 KiB at the end of internal flash), or
* a ``zephyr,settings-partition`` chosen node pointing at any partition,
  including one on external SPI flash.

Boards without either definition fail at build time instead of writing to a
wrong address at runtime. ``AT+JOIN`` supports the RUI3 start, auto-join,
retry-interval and retry-count parameters. ABP remains a planned service.

See ``doc/at-command-compatibility.md`` for the complete command reference and
the precise compatibility limits compared with RUI3.

Requirements
************

* An RZI RAK4631 (``rzi_rak4631``) with its LoRa antenna connected
* A LoRaWAN gateway and network server
* Valid OTAA identifiers and keys

Set keys at runtime with ``AT+DEVEUI``, ``AT+APPEUI`` and ``AT+APPKEY``.
Hardware and the AT UART alias live on the product board.

AT UART selection
*****************

The sample selects its exclusive AT UART adapter with the ``rzi-at-uart``
devicetree alias. It does not reuse ``zephyr,console`` because console and AT
adapter ownership are separate concerns. ``rzi_rak4631`` points the alias to
USB CDC ACM:

.. code-block:: devicetree

   aliases {
       rzi-at-uart = &cdc_acm_uart0;
   };

An application can point the same alias to a ready physical UART instead.
Logging and console output must remain disabled on a UART owned by the AT
service.

Building
********

Build from a west workspace that contains RZI:

.. code-block:: console

   west build -b rzi_rak4631/nrf52840 --sysbuild samples/lorawan/at

Flash ``merged.hex``. See ``doc/boot.md``.
Zephyr discovers RZI from this repository's ``zephyr/module.yml`` metadata.

Example session
***************

Open the USB CDC port at 115200 baud and type:

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
