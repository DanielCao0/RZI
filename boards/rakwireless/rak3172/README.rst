RZI variant of the upstream RAK3172
===================================

The F module uses the independent ``rak3372_f/stm32wle5xx`` target.
The settings below describe the standard ``rzi`` variant.

``rak3172/stm32wle5xx/rzi`` extends Zephyr's standard RAK3172 board.
This target is for the STM32WLE5CC module with a 32 MHz crystal. It does
not describe the RAK3172-T TCXO hardware or the RAK3172-F external flash.

Hardware settings
-----------------

The compatibility baseline is the validated ``rzi_rak3372`` board at
commit ``a2b11db``. RF control, clocks, peripheral defaults, and partitions
are preserved explicitly where the upstream defaults differ.

The upstream 32 MHz HSE crystal / 48 MHz PLL clock tree and LSE timer
are retained. No ``hse-tcxo`` or ``dio3-tcxo-voltage`` is selected:
programming a TCXO voltage is not appropriate for the standard module.
RAK's module datasheet distinguishes the standard, T, and F versions:
https://docs.rakwireless.com/product-categories/wisduo/rak3172-module/datasheet/

The internal ``st,stm32wl-subghz-radio`` remains on SUBGHZSPI. The variant
explicitly defines the validated RF switch settings:
PC13 is high for TX, PB8 is high for RX, and both are low when idle.
The RFO_HP path and 22 dBm hardware PA limit are retained. PB8 must not
be treated as the always-enabled antenna control used by RAK3172-F.
The RF switch truth table is implemented in RAK RUI3's
``component/core/board/rak3172/radio_board_if.c``.

The variant selects LPUART1 on PA2/PA3 at 115200 baud for console and AT,
and disables USART2 because it uses the same pins. USART1 on PB6/PB7
remains available. External I2C2 and SPI1 are disabled until the application
needs them. The inherited LPTIM1 and RTC continue to use LSE.
The board keeps ``led0`` on the green LED (PA0). The customer example
selects its blue/green aliases in its application overlay. These LEDs
require the corresponding carrier wiring. RTC, AES, and the watchdog are
disabled by default; applications can enable the peripherals they use.

Boot and storage
----------------

The upstream two 108 KiB image slots are replaced with the RZI single-slot
layout defined in ``rak3172-rzi-partitions.dtsi``:

* MCUboot: 32 KiB at 0x00000.
* Application: 208 KiB at 0x08000.
* Settings: 8 KiB at 0x3c000.
* LoRa Basics Modem context: a separate 8 KiB at 0x3e000.

Sysbuild selects ``MCUBOOT_MODE_SINGLE_APP``. There is no ``image-1`` and
no FUOTA. Both MCUboot and the application use the same variant DTS.
Changing from the upstream dual-slot map requires a full erase and a
matching merged bootloader/application image; existing NVM is not preserved.

The RZI-owned USP STM32WL patches remain required: the driver enumerates
the on-chip radio, controls its RF switch and RCC reset, and masks/rearms
EXTI44 correctly. The variant alone does not replace those driver patches.
TCXO-reset handling remains conditional and is not enabled on this target.
See ``doc/west-patch.md`` for the patch workflow. Do not clean or reapply
patches over local module changes just to select this board.

Build
-----

From a customer workspace with the RZI module present::

   west build --sysbuild -b rak3172/stm32wle5xx/rzi \
     -d build/app-rak3172-rzi app

The application uses ``boards/rak3172_stm32wle5xx_rzi.conf`` to disable
USB CDC and ``boards/rak3172_stm32wle5xx_rzi.overlay`` for credentials.
Flash ``build/app-rak3172-rzi/merged_rak3172_stm32wle5xx_rzi.hex``.
No explicit ``BOARD_ROOT`` is needed because RZI exports it as a module.

The AT sample has its own physical-UART configuration::

   west build --sysbuild -b rak3172/stm32wle5xx/rzi \
     -d build/at-rak3172-rzi rzi/samples/at

The SDK periodical uplink sample is also listed for this target::

   west build --sysbuild -b rak3172/stm32wle5xx/rzi \
     -d build/uplink-rak3172-rzi rzi/samples/lorawan/periodical_uplink
