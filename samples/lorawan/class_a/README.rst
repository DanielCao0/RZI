.. zephyr:code-sample:: rzi_lorawan_class_a
   :name: RZI LoRaWAN Class A
   :relevant-api: rzi_lorawan

   Join a LoRaWAN network and send periodic uplinks through the RZI C API.

Overview
********

This sample starts the RZI LoRaWAN service, waits until the stack is ready,
joins with OTAA and sends an unconfirmed message every 60 seconds. Callbacks
only update flags and print logs. The application loop does not use a message
queue. It does not include or call the USP/LBM API directly.

Requirements
************

* An RZI RAK4631 (``rzi_rak4631``) with its LoRa antenna connected
* A LoRaWAN gateway and network server
* Valid OTAA identifiers and keys

Replace the placeholder keys in ``app.overlay``. Hardware, partitions and
USP radio compatibility live on the ``rzi_rak4631`` board, not in this sample.

Building
********

Build from a west workspace that contains RZI:

.. code-block:: console

   west build -b rzi_rak4631/nrf52840 --sysbuild samples/lorawan/class_a

Flash ``merged.hex`` from the build directory. See ``doc/boot.md``.
Zephyr discovers RZI from this repository's ``zephyr/module.yml`` metadata.
