# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(
  lorawan.c
  lorawan_feature.c
)

zephyr_library_sources_ifdef(
  CONFIG_RZI_LORAWAN_BACKEND_USP
  backends/usp/lorawan_backend_usp.c
)
zephyr_library_link_libraries_ifdef(
  CONFIG_RZI_LORAWAN_BACKEND_USP
  lbm_compile_definitions
)

zephyr_library_sources_ifdef(CONFIG_RZI_LORAWAN_MULTICAST
  services/multicast/lorawan_multicast.c)
zephyr_library_sources_ifdef(CONFIG_RZI_LORAWAN_FUOTA
  services/fuota/lorawan_fuota.c)
if(CONFIG_RZI_LORAWAN_LINK_CHECK OR CONFIG_RZI_LORAWAN_DEVICE_TIME)
  zephyr_library_sources(mac_commands/lorawan_mac_commands.c)
endif()
zephyr_library_sources_ifdef(CONFIG_RZI_LORAWAN_CHANNEL_SCAN
  services/channel_scan/lorawan_channel_scan.c)
zephyr_library_sources_ifdef(CONFIG_RZI_LORAWAN_CERTIFICATION
  services/certification/lorawan_certification.c)
