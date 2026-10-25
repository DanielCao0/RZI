# SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0

# MCUboot signs and checks the same slot size the AT image is linked for.
if(BOARD MATCHES "^rak3372_f")
	set(_at_flash_overlay "${APP_DIR}/boards/rak3372_f_stm32wle5xx.overlay")
	if(NOT DEFINED mcuboot_DTC_OVERLAY_FILE OR "${mcuboot_DTC_OVERLAY_FILE}" STREQUAL "")
		set(mcuboot_DTC_OVERLAY_FILE "${_at_flash_overlay}")
	elseif(NOT "${_at_flash_overlay}" IN_LIST mcuboot_DTC_OVERLAY_FILE)
		list(APPEND mcuboot_DTC_OVERLAY_FILE "${_at_flash_overlay}")
	endif()
	set(mcuboot_DTC_OVERLAY_FILE "${mcuboot_DTC_OVERLAY_FILE}"
	    CACHE INTERNAL "AT flash map plus board MCUboot overlay" FORCE)
endif()
