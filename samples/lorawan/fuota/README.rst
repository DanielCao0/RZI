.. zephyr:code-sample:: rzi_lorawan_fuota
   :name: RZI LoRaWAN FUOTA
   :relevant-api: rzi_lorawan rzi_fuota

   Join a LoRaWAN network and accept ChirpStack FUOTA deployments through RZI.

Overview
********

This sample starts the stack, waits until it is ready, joins, then sends an
uplink every 5 seconds. The application blocks on semaphores that the event
callback gives; callbacks only signal and print logs.
``rzi_lorawan_init()`` starts FUOTA coordination when
``CONFIG_RZI_LORAWAN_FUOTA`` is enabled. The application registers FUOTA
callbacks to observe session state and completion. RZI and the USP backend
handle clock sync, Class C multicast and image reconstruction.

Clock Synchronization (FPort 202), Remote Multicast Setup (FPort 200) and
Fragmentation (FPort 201) remain inside LoRa Basics Modem.

``application_key`` in the OTAA join config is the LoRaWAN 1.0.x
**GenAppKey**. It must match the ChirpStack device Gen App Key.

Requirements
************

* An RZI RAK4631 (``rak4631/nrf52840/rzi``) or RAK3372-F (``rak3372_f``) with its LoRa antenna connected
* A LoRaWAN gateway and ChirpStack v4
* Device-profile application-layer packages enabled: Clock Sync v1,
  Remote Multicast Setup v1 and Fragmentation v1
* A short expected uplink interval in the device-profile, for example 5 s
* Valid OTAA identifiers, AppKey and GenAppKey

See ``doc/fuota.md`` for the ChirpStack deployment checklist and image-size
limits of the default LBM context partition.

Building
********

From the workspace root:

.. code-block:: console

   west build -p always -b rak4631/nrf52840/rzi --sysbuild rzi/samples/lorawan/fuota

For RAK3372-F use ``-b rak3372_f/stm32wle5xx``. Console is LPUART1.
Flash ``build/merged_rak3372_f_stm32wle5xx.hex``.

Credentials are in ``app.overlay``, with the same properties as
``samples/lorawan/periodical_uplink``; here GenAppKey is required.
Flash ``build/merged_rak4631_nrf52840_rzi.hex``. See ``doc/boot.md``.

FUOTA needs the secondary image slot. This sample builds for
``rak4631/nrf52840/rzi`` and ``rak3372_f`` (slot1 on the MX25L400).
``rak3172/stm32wle5xx/rzi`` is single-slot.

Expected output
***************

On the console UART at 115200 baud, with timestamps omitted:

.. code-block:: console

   <inf> rzi_fuota_sample: RZI LoRaWAN FUOTA sample
   <inf> rzi_fuota_sample: Joining
   <inf> rzi_fuota_sample: Joined
   <inf> rzi_fuota_sample: Uplink sent

When ChirpStack starts a deployment, the multicast and fragmentation setup
arrives as downlinks, then:

.. code-block:: console

   <inf> rzi_fuota_sample: FUOTA transfer started
   <inf> rzi_fuota_sample: FUOTA transferring
   <inf> rzi_fuota_sample: FUOTA finished

The state line uses the ``enum rzi_fuota_state`` name: ``idle``, ``ready``,
``transferring``, ``complete``, ``failed`` or ``applying``.

Troubleshooting
***************

Joined, but no ``FUOTA transfer started``
   The device-profile does not enable all three application-layer packages,
   or the deployment has not reached the multicast session start time yet.

``FUOTA failed: RZI_ERR_*``
   The name comes from ``include/rzi/err.h``. Compare it with the deployment
   status in ChirpStack, and check the image-size limits in ``doc/fuota.md``.

Join problems
   See the troubleshooting section of ``samples/lorawan/periodical_uplink``.

Test payload
************

``test_payload/rzi-fuota-test.bin`` is a 4096-byte ChirpStack upload file.
It starts with the ASCII magic ``RZI1``. After a successful deployment the
``rzi_fuota`` log module prints ``FUOTA image head`` beginning ``52 5a 49 31``.

Regenerate it with:

.. code-block:: console

   python3 samples/lorawan/fuota/test_payload/gen_test_bin.py
