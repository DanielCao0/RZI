.. zephyr:code-sample:: rzi_lorawan_periodical_uplink
   :name: RZI LoRaWAN periodical uplink
   :relevant-api: rzi_lorawan rzi_power

   Join a LoRaWAN network and send periodic uplinks through the RZI C API.

Overview
********

This sample starts the RZI LoRaWAN service, joins with OTAA and sends an
unconfirmed uplink on port 101 right after the join and then every 60
seconds. The payload is a 4-byte big-endian counter that starts at 0 and
increments with every accepted uplink, so gaps on the network server show lost
frames. A failed join is retried after 5 seconds.

It is the RZI counterpart of the usp_zephyr ``lbm/periodical_uplink`` sample,
without the user-button uplink on port 102.

The application blocks on semaphores that the event callback gives, and waits
between uplinks with ``rzi_power_sleep()``, so the CPU sleeps whenever the
stack is idle. It does not include or call the USP/LBM API directly.

Requirements
************

* ``rzi_rak4631``, ``rzi_rak3372`` or ``rzi_rak3362`` with its LoRa antenna connected
* A LoRaWAN gateway and network server in range
* An OTAA device registered on the network server

Configuration
*************

Credentials are in ``app.overlay``. The checked-in values are zeros;
replace them before a radio test:

.. list-table::
   :header-rows: 1

   * - Property
     - Meaning
   * - ``user-lorawan-device-eui``
     - DevEUI, 8 bytes, in the order the network server displays it
   * - ``user-lorawan-join-eui``
     - JoinEUI (AppEUI), 8 bytes
   * - ``user-lorawan-app-key``
     - AppKey, 16 bytes
   * - ``user-lorawan-gen_app-key``
     - GenAppKey, 16 bytes; only used by FUOTA, may stay zero here

The region is ``LORAWAN_REGION`` in ``src/main.c``. The checked-in value is
``RZI_LORAWAN_REGION_EU_868``. The device class is ``LORAWAN_CLASS`` in the
same file, checked in as ``RZI_LORAWAN_CLASS_A``, and applied after the stack
is ready and before the join. The uplink interval, port and retry delay are
``UPLINK_INTERVAL_MS``, ``UPLINK_PORT`` and ``RETRY_DELAY_MS`` in the same
file.

``LORAWAN_ADR`` and ``LORAWAN_DATA_RATE`` are applied in the ``JOINED``
callback. With ``LORAWAN_ADR`` true, ``rzi_lorawan_set_adr(true)`` lets the
network choose the rate. Set it to false to call
``rzi_lorawan_set_data_rate(LORAWAN_DATA_RATE)`` instead; that pins one rate
and turns ADR off. EU868 accepts ``RZI_LORAWAN_DR_0`` through
``RZI_LORAWAN_DR_5``. Either call returns ``RZI_ERR_NOT_JOINED`` before the
device has joined.

Building
********

From the workspace root:

.. code-block:: console

   west build -p always -b rzi_rak4631/nrf52840 --sysbuild rzi/samples/lorawan/periodical_uplink

For RAK3372 use ``-b rzi_rak3372/stm32wle5xx``.

Flash ``build/merged_<board>.hex``, for example
``build/merged_rzi_rak4631_nrf52840.hex``. It contains MCUboot and the signed
application. ``zephyr.hex`` alone has no bootloader. See ``doc/boot.md``.

Low-power variant
=================

``prj_lowpower.conf`` removes logging, ``printk`` and the UART driver. The
enabled console UART keeps its clocks running and dominates sleep current, so
use this variant to measure current consumption:

.. code-block:: console

   west build -p always -b rzi_rak4631/nrf52840 --sysbuild rzi/samples/lorawan/periodical_uplink \
      -- -DEXTRA_CONF_FILE=prj_lowpower.conf

The firmware behaves the same but prints nothing. Check joins and uplinks on
the network server instead.

Expected output
***************

Open the console UART (``zephyr,console`` in the board devicetree) at 115200
baud. After reset the sample prints, with timestamps omitted:

.. code-block:: console

   *** Booting Zephyr OS build ... ***
   <inf> rzi_periodical_uplink: RZI LoRaWAN periodical uplink (60 s) sample
   <inf> rzi_periodical_uplink: Joining
   <inf> rzi_periodical_uplink: Joined
   <inf> rzi_periodical_uplink: Uplink 0 on port 101
   <inf> rzi_periodical_uplink: Uplink sent
   <inf> rzi_periodical_uplink: Uplink 1 on port 101
   <inf> rzi_periodical_uplink: Uplink sent

The pair of lines repeats every 60 seconds; the network server shows payloads
``00000000``, ``00000001`` and so on. When the network server queues
a downlink, the next uplink is followed by a line such as:

.. code-block:: console

   <inf> rzi_periodical_uplink: Downlink: port 2, 3 bytes, RSSI -45 dBm, SNR 38/4 dB

MCUboot prints its own lines before the Zephyr banner. LoRa Basics Modem
traces stay off unless ``CONFIG_USP_LOG_LEVEL_INF=y`` is set.

Troubleshooting
***************

Failed calls print an ``RZI_ERR_*`` name from ``include/rzi/err.h``.

``Join failed, retrying in 5 s`` repeats
   The network server did not accept the join. Check that DevEUI, JoinEUI and
   AppKey match the server byte for byte, that the region matches the gateway,
   and that the gateway is online. The placeholder zero keys never join.

``Credentials failed``, ``LoRaWAN init failed`` or ``Class failed`` right after boot
   Credentials, stack initialization, or the device class was rejected. Check
   the property lengths in ``app.overlay``, plus ``LORAWAN_REGION`` and
   ``LORAWAN_CLASS`` in ``src/main.c``.

``Uplink rejected: RZI_ERR_*``
   The stack refused the uplink. ``RZI_ERR_BUSY`` (the previous uplink is
   still in progress) is expected and is not logged.

No output at all
   The board was flashed with ``zephyr.hex`` instead of the merged image, the
   terminal is on the wrong port or baud rate, or the low-power variant is
   installed.
