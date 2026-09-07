.. zephyr:code-sample:: rzi_lorawan_at
   :name: RZI LoRaWAN AT
   :relevant-api: rzi_lorawan

   RAK RUI3-compatible AT command interface over USB CDC UART.

Overview
********

This sample starts the RZI AT command service on the USB CDC serial port.
The UART transport feeds the transport-independent AT core. The optional
LoRaWAN command package consumes the RZI LoRaWAN C API and follows the command
behaviors, status strings and asynchronous events of the RAK RUI3 AT Command
Manual, so host software written for RUI3 modules can drive it directly.

The relevant configuration switches are ``CONFIG_RZI_AT``,
``CONFIG_RZI_AT_TRANSPORT_UART`` and ``CONFIG_RZI_AT_COMMAND_LORAWAN``.

Implemented subset (OTAA, Class A):

* ``AT``, ``ATZ``, ``ATR``, ``AT+VER``
* ``AT+DEVEUI``, ``AT+APPEUI``, ``AT+APPKEY``
* ``AT+BAND`` (EU433 and LA915 are not supported by the backend)
* ``AT+NJM`` (OTAA only), ``AT+NJS``, ``AT+CLASS`` (Class A only)
* ``AT+CFM``, ``AT+CFS``
* ``AT+JOIN``, ``AT+SEND``, ``AT+RECV``

Asynchronous events: ``+EVT:JOINED``, ``+EVT:JOIN_FAILED_RX_TIMEOUT``,
``+EVT:TX_DONE``, ``+EVT:SEND_CONFIRMED_OK``, ``+EVT:SEND_CONFIRMED_FAILED``
and ``+EVT:RX_1:<rssi>:<snr>:UNICAST:<port>:<payload>``.

Persistence
***********

With ``CONFIG_RZI_AT_NVM`` (enabled in this sample) the parameters set over
AT — DevEUI, JoinEUI, AppKey, band and confirm mode — are stored in flash
through the Zephyr settings subsystem (NVS backend) and survive reboots.
``ATR`` erases them and reboots, restoring the devicetree defaults.

RZI does not hardcode any flash address. The storage area is board-defined
through devicetree, following the standard Zephyr convention:

* a partition labeled ``storage_partition`` in the board's fixed partition
  table (RAK4631 provides the last 32 KiB of internal flash at ``0xf8000``
  via ``nrf52840_partition.dtsi``), or
* a ``zephyr,settings-partition`` chosen node pointing at any partition,
  including one on external SPI flash.

Boards without either definition fail at build time instead of writing to a
wrong address at runtime. ABP and the ``AT+JOIN`` auto-join parameters are
planned services.

Requirements
************

* A RAK4631 board with its LoRa antenna connected
* A LoRaWAN gateway and network server
* Valid OTAA identifiers and keys

The RAK4631 overlay carries sample-only compatibility for the currently
pinned BSP and USP driver; it is not exported as part of the RZI module
interface.

Building
********

Build from a west workspace that contains RZI:

.. code-block:: console

   west build -b rak4631/nrf52840 samples/lorawan/at

Zephyr discovers RZI from this repository's ``zephyr/module.yml`` metadata.

Example session
***************

Open the USB CDC port at 115200 baud and type:

.. code-block:: text

   AT
   OK
   AT+VER=?
   AT+VER=RZI_0.1.0_rak4631
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
