# SPDX-License-Identifier: Apache-2.0
if(CONFIG_RZI)
  zephyr_library()

  zephyr_include_directories(${ZEPHYR_CURRENT_MODULE_DIR}/include)

  zephyr_library_sources_ifdef(
    CONFIG_RZI_LORAWAN
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/lorawan/lorawan.c
  )
  zephyr_library_sources_ifdef(
    CONFIG_RZI_LORAWAN_BACKEND_USP
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/lorawan/backends/usp.c
  )
  zephyr_library_link_libraries_ifdef(
    CONFIG_RZI_LORAWAN_BACKEND_USP
    lbm_compile_definitions
  )
  zephyr_library_sources_ifdef(
    CONFIG_RZI_AT
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/at/core.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/at/parser.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/at/registry.c
  )
  zephyr_library_sources_ifdef(
    CONFIG_RZI_AT
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/at/commands/system.c
  )
  zephyr_library_sources_ifdef(
    CONFIG_RZI_AT_COMMAND_LORAWAN
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/at/commands/lorawan.c
  )
  zephyr_library_sources_ifdef(
    CONFIG_RZI_AT_TRANSPORT_UART
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/at/transports/uart.c
  )
endif()
