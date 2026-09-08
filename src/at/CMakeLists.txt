# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(
  at_core.c
  at_parser.c
  at_registry.c
  commands/at_command_system.c
)

zephyr_library_sources_ifdef(
  CONFIG_RZI_AT_COMMAND_LORA
  commands/lora/at_command_lora.c
  commands/lora/at_command_lora_p2p.c
)

if(CONFIG_RZI_AT_COMMAND_LORAWAN)
  zephyr_library_sources(
    commands/lorawan/at_command_lorawan.c
    commands/lorawan/at_command_lorawan_key_id.c
    commands/lorawan/at_command_lorawan_join_send.c
    commands/lorawan/at_command_lorawan_network_management.c
  )

  zephyr_library_sources_ifdef(CONFIG_RZI_LORAWAN_MULTICAST
    commands/lorawan/at_command_lorawan_multicast.c)
  zephyr_library_sources_ifdef(CONFIG_RZI_LORAWAN_CERTIFICATION
    commands/lorawan/at_command_lorawan_certification.c)
  zephyr_library_sources_ifdef(CONFIG_RZI_LORAWAN_LINK_CHECK
    commands/lorawan/at_command_lorawan_supplementary.c)
endif()

zephyr_library_sources_ifdef(
  CONFIG_RZI_AT_ADAPTER_UART
  adapters/at_adapter_uart.c
)
