.. zephyr:code-sample:: rzi_lorawan_fuota
   :name: RZI LoRaWAN FUOTA
   :relevant-api: rzi_lorawan rzi_fuota

   Join a LoRaWAN network and accept ChirpStack FUOTA deployments through RZI.

Overview
********

This sample starts the stack, waits until it is ready, joins, then sends an
uplink every 5 seconds. Callbacks only update flags and print logs. The
application loop does not use a message queue.
``rzi_lorawan_start()`` starts FUOTA coordination when
``CONFIG_RZI_LORAWAN_FUOTA`` is enabled. The application registers FUOTA
callbacks to observe session state and completion. RZI and the USP backend
handle clock sync, Class C multicast and image reconstruction.

Clock Synchronization (FPort 202), Remote Multicast Setup (FPort 200) and
Fragmentation (FPort 201) remain inside LoRa Basics Modem.

``application_key`` in the OTAA join config is the LoRaWAN 1.0.x
**GenAppKey**. It must match the ChirpStack device Gen App Key.

Requirements
************

* An RZI RAK4631 (``rzi_rak4631``) with its LoRa antenna connected
* A LoRaWAN gateway and ChirpStack v4
* Device-profile application-layer packages enabled: Clock Sync v1,
  Remote Multicast Setup v1 and Fragmentation v1
* A short expected uplink interval in the device-profile, for example 5 s
* Valid OTAA identifiers, AppKey and GenAppKey

See ``doc/fuota.md`` for the ChirpStack deployment checklist and image-size
limits of the default LBM context partition.

Building
********

Build from a west workspace that contains RZI:

.. code-block:: console

   west build -b rzi_rak4631/nrf52840 --sysbuild samples/lorawan/fuota

Credentials are in ``app.overlay``. Flash ``merged.hex``. See ``doc/boot.md``.

Test payload
************

``test-payload/rzi-fuota-test.bin`` is a 4096-byte ChirpStack upload file.
It starts with the ASCII magic ``RZI1``. After a successful deployment the
``rzi_fuota`` log module prints ``FUOTA image head`` beginning ``52 5a 49 31``.

Regenerate it with:

.. code-block:: console

   python3 samples/lorawan/fuota/test-payload/gen_test_bin.py
