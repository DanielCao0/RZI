.. zephyr:code-sample:: rzi_lorawan_low_power
   :name: RZI LoRaWAN Class A low power
   :relevant-api: rzi_lorawan rzi_power

   Event-driven Class A periodical uplink. The application does not
   compute or sleep across RX1/RX2.

Overview
********

This sample follows the official Class A low-power shape:

* Semtech USP ``periodical_uplink`` with ``CONFIG_USP_MAIN_THREAD=y``:
  join on READY, send on JOINED, period timer for the next uplink, main
  thread idle. USP's engine thread already calls
  ``smtc_modem_run_engine()`` and sleeps for the returned
  ``sleep_time_ms``.
* RAK `RUI3-Best-Practice <https://github.com/RAKWireless/RUI3-Best-Practice>`_:
  ``lpm.set(1)``, timer callback sends, empty loop.
* ST AN5406 ``LoRaWAN_End_Node``: sequencer idle enters STOP; the stack
  owns the receive windows.

RZI has no LBM alarm API, so the period uses Zephyr ``k_work_delayable``
(the same role as ``smtc_modem_alarm_start_timer()`` / RUI3
``api.system.timer``). ``rzi_power_set_policy(SUSPEND)`` is the RUI3
``lpm.set(1)`` equivalent.

``samples/lorawan/class_a`` is the simpler API walkthrough. Use this
sample when measuring current.

Requirements
************

* ``rzi_rak4631/nrf52840`` or ``rzi_rak3372/stm32wle5xx``
* LoRa antenna, gateway, and network server
* Valid OTAA credentials in ``app.overlay``

Building
********

.. code-block:: console

   west build -b rzi_rak3372/stm32wle5xx --sysbuild samples/lorawan/low_power
   west build -b rzi_rak4631/nrf52840 --sysbuild samples/lorawan/low_power

Flash ``merged.hex``. See ``doc/boot.md`` and ``doc/power.md``.

Current measurement
*******************

UART logging keeps clocks up. Semtech's official overlay is
``prj_lowpower.conf``:

.. code-block:: console

   west build -b rzi_rak3372/stm32wle5xx --sysbuild samples/lorawan/low_power -- \
     -DEXTRA_CONF_FILE=prj_lowpower.conf

Expect short current spikes at TX and at RX1/RX2, and a low floor
between them (STOP2 on 3372, System ON idle on 4631). This sample does
not claim a milliamp target.
