# SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BOARD_RAK4631_NRF52840_RZI)
  zephyr_library()
  zephyr_library_sources(regout.c)
endif()
