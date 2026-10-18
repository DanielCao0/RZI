.. zephyr:code-sample:: rzi_lora_rf_certification
   :name: RZI LoRa RF certification
   :relevant-api: rzi_lora

   Step through the transmit configurations used for ETSI, FCC or ARIB
   radio certification.

Overview
********

This sample is the RZI port of the Semtech USP ``rf_certification`` example.
It walks through a fixed list of LoRa and FSK configurations for one
regulatory region. On each step it transmits packets back to back through
``rzi_lora_send()``, so a test lab can measure output power, occupied
bandwidth, spurious emissions and hopping behaviour.

Each packet starts with ``RF_CERT``, carries a byte ramp, and ends with a
32-bit big-endian packet counter.

The RZI boards have no user button, so the sample moves to the next step
on a timer instead of a button press. It can also stay on a single step
until reset.

The USP raw LoRa backend runs on the LoRa Basics Modem test API. The sample
therefore starts the LoRaWAN service to bring up the modem, but never joins.

This sample helps you prepare for certification. It does not guarantee
approval: the result depends on the board design and on accredited lab
testing.

Test sequences
**************

The startup log prints the numbered step list for the selected region.

ETSI, 15 steps at 14 dBm, 255-byte payload
   LoRa SF7 and SF12 at 125 kHz on 863.1, 868.3, 868.5 and 866.5 MHz, plus
   SF7 at 250 kHz on 868.3 MHz. Then FSK on 863.1, 866.5, 868.1, 868.3,
   868.5 and 869.9 MHz.

FCC, 17 steps at 22 dBm, US915 maximum payload per data rate
   DTS mode: SF8 at 500 kHz on 903.0, 909.4 and 914.2 MHz.
   Hybrid mode: SF7 and SF10 at 125 kHz on 902.3, 908.7 and 914.9 MHz.
   FHSS sweep: SF10 to SF7, hopping over 64 channels 200 kHz apart from
   902.3 MHz.
   Hybrid sweep: SF10 to SF7, hopping over the first 8 of those channels.
   Sweeps visit every channel once, in a new random order each round.

ARIB, 9 steps at 9 dBm, 255-byte payload
   LoRa SF7 and SF12 at 125 kHz on 920.6, 922.0 and 923.4 MHz, then FSK on
   the same three frequencies.

FSK uses the LoRaWAN FSK settings of the LoRa Basics Modem test mode:
50 kbps with 25 kHz deviation.

Differences from the USP example
================================

* No LR-FHSS or FLRC steps. The RZI raw LoRa API exposes only LoRa and
  FSK, and the SX1262 and STM32WL radios have no FLRC.
* No listen-before-talk on ARIB steps. The RZI API has no LBT transmit
  option. If your ARIB test plan requires LBT, test it separately.
* No notification receiver mode.

Requirements
************

* ``rzi_rak4631``, ``rzi_rak3372`` or ``rzi_rak3362``
* A conducted setup (cable and attenuator) or an antenna in a shielded
  room, connected to the measuring instrument

The sample transmits continuously at up to 22 dBm with no duty-cycle limit.
Run it only in a shielded setup or into a dummy load.

Configuration
*************

The sample's own Kconfig options:

.. list-table::
   :header-rows: 1

   * - Option
     - Default
     - Meaning
   * - ``CONFIG_APP_RF_CERT_REGION_ETSI``, ``_FCC``, ``_ARIB``
     - ETSI
     - Region whose test sequence runs
   * - ``CONFIG_APP_RF_CERT_STEP_S``
     - ``60``
     - Seconds spent on each step
   * - ``CONFIG_APP_RF_CERT_ONLY_STEP``
     - ``0``
     - ``0`` runs the whole sequence once; ``N`` transmits on step ``N``
       until reset

For example, the FCC sequence with 2 minutes per step:

.. code-block:: console

   west build -p always -b rzi_rak4631/nrf52840 --sysbuild rzi/samples/lora/rf_certification \
      -- -DCONFIG_APP_RF_CERT_REGION_FCC=y -DCONFIG_APP_RF_CERT_STEP_S=120

Or ETSI step 2 (SF12 on 863.1 MHz) until reset:

.. code-block:: console

   west build -p always -b rzi_rak4631/nrf52840 --sysbuild rzi/samples/lora/rf_certification \
      -- -DCONFIG_APP_RF_CERT_ONLY_STEP=2

A packet that is still in flight finishes before the next step starts, so
an SF12 step can run a few seconds longer than configured.

RAK3372 uses fixed STM32WL power-amplifier settings. Measure its output
instead of trusting the requested power.

Building
********

From the workspace root:

.. code-block:: console

   west build -p always -b rzi_rak4631/nrf52840 --sysbuild rzi/samples/lora/rf_certification

For RAK3372 use ``-b rzi_rak3372/stm32wle5xx``. Flash
``build/merged_<board>.hex``. See ``doc/boot.md``.

Expected output
***************

On the console UART at 115200 baud, with timestamps omitted and the step
list shortened:

.. code-block:: console

   <inf> rzi_rf_certification: RZI RF certification: ETSI, 15 steps
   <inf> rzi_rf_certification:  1  LoRa  863.100 MHz  SF7  BW125  14 dBm  255 B
   <inf> rzi_rf_certification:  2  LoRa  863.100 MHz  SF12 BW125  14 dBm  255 B
   ...
   <inf> rzi_rf_certification: 15  FSK   869.900 MHz  50 kbps     14 dBm  255 B
   <inf> rzi_rf_certification: Step 1 of 15:
   <inf> rzi_rf_certification:  1  LoRa  863.100 MHz  SF7  BW125  14 dBm  255 B
   <inf> rzi_rf_certification: Step 1: 23 packets
   ...
   <inf> rzi_rf_certification: Step 1 done: 139 packets
   <inf> rzi_rf_certification: Step 2 of 15:
   ...
   <inf> rzi_rf_certification: Sequence complete: 1874 packets

Progress is logged every 10 seconds rather than for every packet. Reset the
board to run the sequence again.

Troubleshooting
***************

``LoRaWAN ready timed out``
   The radio did not answer. Check that the board target matches the
   hardware.

``Step N setup failed``, ``Step N start failed`` or ``Packet N rejected``
   The line names the code from ``include/rzi/err.h``. ``RZI_ERR_INVALID``
   usually means the frequency or power is outside what the radio supports.

``Packet N not sent``
   The radio never reported the end of the transmission. Check the radio
   wiring and supply.

The FSK steps show the wrong deviation or bitrate
   FSK parameters come from the LoRa Basics Modem test mode, not from
   ``struct rzi_lora_config``.
