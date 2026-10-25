# SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0

config RZI_MCUBOOT
	bool "RZI MCUboot device boot"
	imply BOOTLOADER_MCUBOOT
	help
	  Official RZI hardware images boot through MCUboot. Build with
	  sysbuild; the RZI samples and product app already do. Flash the
	  merged image, not zephyr.hex. Partition tables stay on the RZI
	  board or variant (rak4631/nrf52840/rzi,
	  rak3172/stm32wle5xx/rzi, ...). Enable CONFIG_IMG_MANAGER in the
	  application when FUOTA should install a reconstructed image.

config RZI_MCUBOOT_DUAL_SLOT
	bool "MCUboot secondary slot (image-1)"
	depends on RZI_MCUBOOT
	default y if BOARD_RAK3372_F
	help
	  The product board has slot1_partition. LoRaWAN FUOTA writes a
	  signed image there. rak3372_f keeps that slot on the
	  on-module MX25L400. Single-slot boards such as rak3172/stm32wle5xx/rzi
	  (256 KiB internal, no external NOR) leave this off.
