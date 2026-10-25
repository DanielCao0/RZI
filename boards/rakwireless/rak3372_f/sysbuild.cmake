# SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0

# Runs after images are added and before their cache files are written.
set(mcuboot_DTC_OVERLAY_FILE "${CMAKE_CURRENT_LIST_DIR}/mcuboot.overlay"
    CACHE INTERNAL "RAK3372-F MCUboot console on USART2" FORCE)
