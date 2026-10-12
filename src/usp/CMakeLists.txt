# SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(usp_platform.c)
zephyr_library_link_libraries(lbm_compile_definitions)
zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR})
