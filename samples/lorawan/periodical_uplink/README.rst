.. zephyr:code-sample:: rzi_lorawan_periodical_uplink
   :name: RZI LoRaWAN periodical uplink
   :relevant-api: rzi_lorawan

   Join a LoRaWAN network and send periodic uplinks through the RZI C API.

Overview
********

This sample starts the RZI LoRaWAN service, joins with OTAA and sends an
unconfirmed uplink on port 101 right after joining and applying the class
and data-rate settings. Each completed exchange (``TXDONE``) schedules the
next uplink after 60 seconds. The payload is a 4-byte big-endian counter that
starts at 0 on each boot and increments with every accepted uplink request.
Acceptance and ``TXDONE`` do not guarantee delivery to the network server;
these uplinks are unconfirmed. A rejected request or failed join is retried
after 5 seconds. Regional backoff and duty-cycle limits still apply.

It is the RZI counterpart of the usp_zephyr ``lbm/periodical_uplink`` sample,
without the user-button uplink on port 102.

``main()`` stores credentials through RZI, registers the callback, and starts
the LoRaWAN service. It then returns; RZI and Zephyr continue running the
modem, event dispatcher, and delayed work. No application thread blocks
waiting for a radio event. The sample uses only the RZI public API.

LBM lifecycle
=============

The event sequence follows the LBM lifecycle:

.. list-table::
   :header-rows: 1

   * - Phase
     - Application action
   * - ``main()``
     - Store DevEUI, JoinEUI, keys, and OTAA activation; initialize the service.
   * - ``READY``
     - Schedule the first join request after backend setup completes.
   * - ``JOINED``
     - Apply Class and ADR or fixed data rate, then schedule the first uplink.
   * - ``TXDONE``
     - Log the result and schedule the next uplink after 60 seconds.
   * - ``JOIN_FAILED``
     - Schedule a new join request after 5 seconds.

In the USP backend, RZI applies the stored credentials and region while
handling LBM's ``RESET`` event, before publishing ``READY``. Thus the RZI
credential setters in ``main()`` do not call an uninitialized LBM instance.
LBM's ``smtc_modem_set_class()`` and ``smtc_modem_adr_set_profile()`` reject
calls without a joined session. Class and ADR/data-rate settings therefore
live in the ``JOINED`` case, including for subsequent join events. A failed
setting is logged and prevents the first uplink from being scheduled.
Callbacks must not sleep or wait for another modem event.

The reference sources in this workspace are
``modules/lib/usp/examples/main_examples/periodical_uplink_example/main_periodical_uplink.c``
and ``modules/lib/usp/protocols/lbm_lib/smtc_modem_core/smtc_modem.c``.
The LBM example uses its alarm API for periodic requests. This RZI sample
uses Zephyr delayed work and starts the interval at ``TXDONE``, avoiding an
application request while the previous exchange is still pending.

After a failed join, the sample queries
``rzi_lorawan_get_join_backoff(&backoff_s)``. When a restriction remains, it
prints ``Join backoff remaining: N s``. This value is a snapshot of the
remaining join-backoff restriction, not the time until the next transmission.
It excludes LBM's random delay, other radio scheduling, and the application
retry delay. An unsupported query is skipped; other query failures are
logged. A zero value does not mean the next request transmits immediately.

RZI requests one join attempt (``attempts = 0`` means no additional retries).
``JOIN_FAILED`` is the end of that RZI join sequence; the service stops LBM's
queued retry before delivering the event. The sample then schedules a new
request. The 5-second delay is an application retry delay, not a promise
that a Join-Request is transmitted every 5 seconds. Logs therefore report
``retry scheduled`` without predicting the radio transmission time. LBM controls radio
scheduling and regional backoff. Rejected uplink requests keep their counter
and retry after the same application delay.

Requirements
************

* A supported board with its LoRa antenna connected:
  ``rak4631/nrf52840/rzi``, ``rak3172/stm32wle5xx/rzi``, or
  ``rak3362/nrf54l15/cpuapp``
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
same file. Set it to ``RZI_LORAWAN_CLASS_A``, ``RZI_LORAWAN_CLASS_B``, or
``RZI_LORAWAN_CLASS_C``. The checked-in value is ``RZI_LORAWAN_CLASS_A``.
The uplink interval, port and retry delay are ``UPLINK_INTERVAL_MS``,
``UPLINK_PORT`` and ``RETRY_DELAY_MS`` in the same file.

``EU868_DUTY_CYCLE_CHECK`` wraps the application-side duty-cycle check and
additional wait-time logging in a separate conditional block. Set it to ``0``
to disable both. When enabled and ``LORAWAN_REGION`` is EU868, the sample
queries the status before each uplink. If limited, it does not submit the
uplink or increment the counter; instead, it schedules another check after
the reported wait. Query failures are retried after ``RETRY_DELAY_MS``.
The sample also logs the remaining wait when a limitation begins.
This is a snapshot of the duty-cycle restriction, not a guarantee of
immediate transmission after submission. An accepted uplink remains
scheduled by LBM. Other regions retain the original sending behavior and
the ``limited`` / ``clear`` event logs.

``LORAWAN_CLASS``, ``LORAWAN_ADR`` and ``LORAWAN_DATA_RATE`` are applied after
join, in the ``JOINED`` callback. The modem rejects those changes before
the device has joined. Class A,
B and C all use ``rzi_lorawan_set_class()``. With Class B, the modem requests
network time and ping-slot information, then searches for a beacon.
``CLASS_B: ACTIVE`` means the beacon is locked. With ``LORAWAN_ADR`` true,
the network chooses the data rate. Set it to false to use
``LORAWAN_DATA_RATE`` instead; that call disables ADR. EU868 accepts
``RZI_LORAWAN_DR_0`` through ``RZI_LORAWAN_DR_5``.

Building
********

From the workspace root:

.. code-block:: console

   west build -p always -b rak4631/nrf52840/rzi --sysbuild rzi/samples/lorawan/periodical_uplink

For RAK3172, build and flash with J-Link:

.. code-block:: console

   west build --sysbuild -b rak3172/stm32wle5xx/rzi \
      -d build/uplink-rak3172-rzi rzi/samples/lorawan/periodical_uplink
   west flash -d build/uplink-rak3172-rzi -r jlink

RAK3172 uses LPUART1 on PA2/PA3 at 115200 baud for its console.

Flash ``build/merged_<board>.hex``, for example
``build/merged_rak4631_nrf52840_rzi.hex``. It contains MCUboot and the signed
application. ``zephyr.hex`` alone has no bootloader. See ``doc/boot.md``.

Low-power variant
=================

``prj_lowpower.conf`` removes logging, ``printk`` and the UART driver. The
enabled console UART keeps its clocks running and dominates sleep current, so
use this variant to measure current consumption:

.. code-block:: console

   west build -p always -b rak4631/nrf52840/rzi --sysbuild rzi/samples/lorawan/periodical_uplink \
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
   <inf> rzi_periodical_uplink: Join request accepted; waiting for Join-Accept
   <inf> rzi_periodical_uplink: Joined
   <inf> rzi_periodical_uplink: Uplink 0 accepted on port 101
   <inf> rzi_periodical_uplink: TXDONE
   <inf> rzi_periodical_uplink: Uplink 1 accepted on port 101
   <inf> rzi_periodical_uplink: TXDONE

The next request is scheduled 60 seconds after ``TXDONE``; the server shows
payloads ``00000000``, ``00000001`` and so on. When the network server queues
a downlink, the next uplink is followed by a line such as:

.. code-block:: console

   <inf> rzi_periodical_uplink: DOWNDATA: port 2, 3 bytes, RSSI -45 dBm, SNR 38/4 dB

MCUboot prints its own lines before the Zephyr banner. LoRa Basics Modem
traces stay off unless ``CONFIG_USP_LOG_LEVEL_INF=y`` is set.

Troubleshooting
***************

Failed calls print an ``RZI_ERR_*`` name from ``include/rzi/err.h``.

``Join attempt failed; retry scheduled`` repeats
   The network server did not accept the join. Check that DevEUI, JoinEUI and
   AppKey match the server byte for byte, that the region matches the gateway,
   and that the gateway is online. The placeholder zero keys never join.

Configuration or initialization fails right after boot
   Each failed call logs the setting it could not apply, such as
   ``Device EUI configuration failed`` or ``AppKey configuration failed``.
   Check property lengths in ``app.overlay`` and ``LORAWAN_REGION`` in
   ``src/main.c`` for ``LoRaWAN init failed``.

``Class configuration failed`` after joining
   The modem rejected the class change. It is accepted only after the device
   has joined. For Class B, ``CLASS_B: ACTIVE`` means the beacon is locked.
   A gateway that does not send beacons never prints that line.

``Uplink request rejected: RZI_ERR_*; retry scheduled``
   The stack refused the request. The same counter is retried after 5 seconds.
   A successful request is followed by ``TXDONE`` before the next interval
   starts. A ``TXDONE`` error is logged; the next request uses the next counter.

No output at all
   The board was flashed with ``zephyr.hex`` instead of the merged image, the
   terminal is on the wrong port or baud rate, or the low-power variant is
   installed.
