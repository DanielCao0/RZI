# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(
  lorawan.c
  lorawan_config.c
  lorawan_mac_commands.c
  lorawan_multicast.c
  lorawan_certification.c
)

zephyr_library_sources_ifdef(
  CONFIG_RZI_LORAWAN_BACKEND_USP
  backend/usp/lorawan_backend_usp.c
  backend/usp/lorawan_backend_usp_features.c
)
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
