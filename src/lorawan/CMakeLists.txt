# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(
  lorawan.c
  api/lorawan_network.c
  api/lorawan_channel.c
  api/lorawan_class_b.c
  api/lorawan_session.c
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
