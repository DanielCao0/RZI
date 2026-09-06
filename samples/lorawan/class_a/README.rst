.. zephyr:code-sample:: rzi_lorawan_class_a
   :name: RZI LoRaWAN Class A
   :relevant-api: rzi_lorawan

   Join a LoRaWAN network and send periodic uplinks through the RZI C API.

Overview
********

This sample initializes the RZI LoRaWAN service, joins with OTAA and sends an
unconfirmed message every 60 seconds. It demonstrates the public RZI event API;
it does not include or call the USP/LBM API directly.

Requirements
************

* A RAK4631 board with its LoRa antenna connected
* A LoRaWAN gateway and network server
* Valid OTAA identifiers and keys

Before running, copy the board overlay and replace its zero-valued DevEUI,
JoinEUI and keys. Keep credentials in a local overlay outside source control.
The RAK4631 overlay carries sample-only compatibility for the currently pinned
BSP and USP driver; it is not exported as part of the RZI module interface.

Building
********

Build from a west workspace that contains RZI:

.. code-block:: console

   west build -b rak4631/nrf52840 samples/lorawan/class_a

Zephyr discovers RZI from this repository's ``zephyr/module.yml`` metadata.
