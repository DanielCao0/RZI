.. zephyr:code-sample:: rzi_lora_tx_cw
   :name: RZI LoRa continuous wave
   :relevant-api: rzi_lora

   Transmit an unmodulated carrier for RF measurements.

Overview
********

This sample transmits a continuous-wave (CW) carrier at a fixed frequency and
power through ``rzi_lora_test_cw()``, then stops it with
``rzi_lora_test_stop()``. Use it with a spectrum analyzer or power meter to
check output power, frequency accuracy and the matching network, for example
during RF certification.

The USP raw LoRa backend runs on the LoRa Basics Modem test API. The sample
therefore starts the LoRaWAN service to bring up the modem, but never joins.

Requirements
************

* ``rzi_rak4631``, ``rzi_rak3372`` or ``rzi_rak3362``
* An antenna, or a cable and attenuator to the measuring instrument

A CW carrier occupies the channel continuously. Transmit only into a
shielded setup or a dummy load, or where local regulations allow it.

Configuration
*************

The sample's own Kconfig options:

.. list-table::
   :header-rows: 1

   * - Option
     - Default
     - Meaning
   * - ``CONFIG_APP_TX_CW_FREQUENCY_HZ``
     - ``868100000``
     - Carrier frequency in hertz
   * - ``CONFIG_APP_TX_CW_POWER_DBM``
     - ``14``
     - Transmit power in dBm, -9 to 22
   * - ``CONFIG_APP_TX_CW_DURATION_S``
     - ``10``
     - Seconds before the carrier stops; ``0`` keeps it on until reset

Set them on the command line, for example 915 MHz at 20 dBm until reset:

.. code-block:: console

   west build -p always -b rzi_rak4631/nrf52840 --sysbuild rzi/samples/lora/tx_cw \
      -- -DCONFIG_APP_TX_CW_FREQUENCY_HZ=915000000 -DCONFIG_APP_TX_CW_POWER_DBM=20 \
      -DCONFIG_APP_TX_CW_DURATION_S=0

The radio, not this option, sets the real power limit. RAK3372 uses fixed
STM32WL power-amplifier settings, so measure its output instead of trusting
the requested value.

Building
********

From the workspace root:

.. code-block:: console

   west build -p always -b rzi_rak4631/nrf52840 --sysbuild rzi/samples/lora/tx_cw

For RAK3372 use ``-b rzi_rak3372/stm32wle5xx``. Flash
``build/merged_<board>.hex``. See ``doc/boot.md``.

Expected output
***************

On the console UART at 115200 baud, with timestamps omitted:

.. code-block:: console

   <inf> rzi_tx_cw: RZI continuous-wave test: 868100000 Hz, 14 dBm
   <inf> rzi_tx_cw: Carrier on for 10 s
   <inf> rzi_tx_cw: Carrier off

The instrument shows a single tone at the configured frequency while
``Carrier on`` is active. Reset the board to transmit again.

Troubleshooting
***************

``LoRaWAN ready timed out``
   The radio did not answer. Check that the board target matches the
   hardware.

``LoRa init failed`` or ``Carrier start failed``
   The line names the code from ``include/rzi/err.h``. ``RZI_ERR_INVALID``
   usually means the frequency or power is outside what the radio supports.

The tone is offset from the configured frequency
   A small offset is the crystal tolerance. Measure it here before tuning
   anything in software.
