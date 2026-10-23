# SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(power.c)

if(CONFIG_SOC_SERIES_NRF52 OR CONFIG_SOC_SERIES_NRF54L)
  zephyr_library_sources(backend/power_board_nordic.c)
else()
  zephyr_library_sources(backend/power_board_generic.c)
endif()
