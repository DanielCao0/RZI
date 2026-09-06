.. zephyr:code-sample:: rzi_lorawan_at
   :name: RZI LoRaWAN AT
   :relevant-api: rzi_lorawan

   RAK RUI3-compatible AT command interface over USB CDC UART.

Overview
********

This sample starts the RZI AT command service on the USB CDC serial port.
The service is a consumer of the RZI LoRaWAN C API and follows the command
behaviors, status strings and asynchronous events of the RAK RUI3 AT Command
Manual, so host software written for RUI3 modules can drive it directly.

Implemented subset (OTAA, Class A):

* ``AT``, ``ATZ``, ``AT+VER``
* ``AT+DEVEUI``, ``AT+APPEUI``, ``AT+APPKEY``
* ``AT+BAND`` (EU433 and LA915 are not supported by the backend)
* ``AT+NJM`` (OTAA only), ``AT+NJS``, ``AT+CLASS`` (Class A only)
* ``AT+CFM``, ``AT+CFS``
* ``AT+JOIN``, ``AT+SEND``, ``AT+RECV``

Asynchronous events: ``+EVT:JOINED``, ``+EVT:JOIN_FAILED_RX_TIMEOUT``,
``+EVT:TX_DONE``, ``+EVT:SEND_CONFIRMED_OK``, ``+EVT:SEND_CONFIRMED_FAILED``
and ``+EVT:RX_1:<rssi>:<snr>:UNICAST:<port>:<payload>``.

Credentials and the region live in RAM. The defaults come from the
``zephyr,user`` devicetree node and updates apply to the next ``AT+JOIN``;
a reboot restores the devicetree defaults. Flash persistence, ABP and the
``AT+JOIN`` auto-join parameters are planned services.

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
