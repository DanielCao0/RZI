# SPDX-License-Identifier: Apache-2.0

config RZI_MCUBOOT
	bool "RZI MCUboot device boot"
	imply BOOTLOADER_MCUBOOT
	help
	  Official RZI hardware images boot through MCUboot. Build with
	  sysbuild; the RZI samples and product app already do. Flash the
	  merged image, not zephyr.hex. Partition tables stay on the RZI
	  product board (rzi_rak4631, rzi_rak3372, ...). Enable
	  CONFIG_IMG_MANAGER in the application when FUOTA should install
	  a reconstructed image.

config RZI_MCUBOOT_DUAL_SLOT
	bool "MCUboot secondary slot (image-1)"
	depends on RZI_MCUBOOT
	default y if BOARD_RZI_RAK4631
	help
	  The product board has slot1_partition. LoRaWAN FUOTA and RSUP
	  type-1 UART update write a signed image there. Single-slot
	  boards such as rzi_rak3372 (256 KiB) leave this off.
