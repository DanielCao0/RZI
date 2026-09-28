# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(
  lorawan.c
  api/lorawan_network.c
  api/lorawan_channel.c
  api/lorawan_class_b.c
  api/lorawan_session.c
  api/lorawan_credentials.c
  api/lorawan_mac_commands.c
  api/lorawan_multicast.c
  api/lorawan_certification.c
)

zephyr_library_sources_ifdef(
  CONFIG_RZI_LORAWAN_BACKEND_USP
  backend/usp/lorawan_backend_usp.c
  backend/usp/lorawan_backend_usp_network.c
  backend/usp/lorawan_backend_usp_class_b.c
  backend/usp/lorawan_backend_usp_mac.c
  backend/usp/lorawan_backend_usp_multicast.c
  backend/usp/lorawan_backend_usp_certification.c
  backend/usp/lorawan_backend_usp_session.c
)
if(CONFIG_RZI_LORAWAN_BACKEND_USP)
  set(RZI_USP_SE_DIR
      ${ZEPHYR_USP_MODULE_DIR}/protocols/lbm_lib/smtc_modem_core/smtc_modem_crypto/smtc_secure_element)
  if(NOT EXISTS ${RZI_USP_SE_DIR}/smtc_secure_element.h)
    message(FATAL_ERROR "LBM secure-element header not found: ${RZI_USP_SE_DIR}")
  endif()
  zephyr_library_include_directories(${RZI_USP_SE_DIR})
endif()
zephyr_library_link_libraries_ifdef(
  CONFIG_RZI_LORAWAN_BACKEND_USP
  lbm_compile_definitions
)
zephyr_library_sources_ifdef(
  CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR
  backend/zephyr/lorawan_backend_zephyr.c
  backend/zephyr/lorawan_backend_zephyr_features.c
)

zephyr_library_sources_ifdef(CONFIG_RZI_LORAWAN_FUOTA
  services/fuota/lorawan_fuota.c)

if(CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR)
  set(RZI_ZEPHYR_MIB_C "${CMAKE_CURRENT_LIST_DIR}/backend/zephyr/lorawan_zephyr_mib.c")
  cmake_language(EVAL CODE
    "cmake_language(DEFER DIRECTORY \"${CMAKE_SOURCE_DIR}\" CALL target_sources loramac-node PRIVATE \"${RZI_ZEPHYR_MIB_C}\")"
  )
endif()
