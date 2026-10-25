RZI variant of the upstream RAK4631
===================================

The ``rak4631/nrf52840/rzi`` target extends the upstream Zephyr board.
It uses the USP SX1262 binding and RF control GPIOs, adds LED aliases,
and defines its flash map in ``rak4631-rzi-partitions.dtsi``. MCUboot and
the application select the same variant and therefore the same flash map.
Hardware defaults follow the validated ``rzi_rak4631`` board at commit
``a2b11db``, with explicit overrides for differing upstream defaults.
An ``EARLY`` initialization restores UICR REGOUT0 to 3.3 V when it is
at its erased default and the nRF52840 is in high-voltage supply mode,
then resets to apply the setting. Both MCUboot and the application include
this initialization, before GPIO and radio drivers run. Existing nondefault
REGOUT0 values are preserved. The upstream early hook remains available.

Sysbuild defaults to overwrite-only upgrades. RZI applications with
``CONFIG_RZI_MCUBOOT=y`` enable the secondary slot by default.

The variant keeps ADC, PWM, 802.15.4, and WisBlock connector devices disabled
by default. UART0 uses the non-DMA UART peripheral and I2C1 uses TWI.
Automatic USB serial setup from the upstream board is disabled; applications
that need USB CDC, including the AT and FUOTA samples, enable it explicitly.

From a customer workspace with the RZI module present::

   west build --sysbuild -b rak4631/nrf52840/rzi \
     -d build/app-rzi-variant app

The SDK periodical uplink sample can be built independently of the customer
application::

   west build --sysbuild -b rak4631/nrf52840/rzi \
     -d build/rzi-variant-uplink rzi/samples/lorawan/periodical_uplink

Its merged image is
``build/rzi-variant-uplink/merged_rak4631_nrf52840_rzi.hex``.

No explicit ``BOARD_ROOT`` is needed: RZI exports it through
``zephyr/module.yml``. The example application keeps credentials and region
selection in ``app/boards/rak4631_nrf52840_rzi.overlay``. Its USB console
references the upstream ``board_cdc_acm_uart`` node.

Flash ``build/app-rzi-variant/merged_rak4631_nrf52840_rzi.hex`` for initial
installation.
