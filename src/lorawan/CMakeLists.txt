# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(
  core/lorawan.c
  core/lorawan_config.c
  core/lorawan_mac_commands.c
  core/lorawan_multicast.c
  core/lorawan_certification.c
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
