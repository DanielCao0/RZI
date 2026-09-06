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
endif()
