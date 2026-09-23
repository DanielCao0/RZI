# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(
  at.c
  at_parser.c
  at_registry.c
  commands/at_command_system.c
)

if(CONFIG_RZI_AT_COMMAND_LORAWAN OR CONFIG_RZI_AT_COMMAND_LORA)
  zephyr_library_sources(commands/at_network_mode.c)
endif()

zephyr_library_sources_ifdef(
  CONFIG_RZI_AT_COMMAND_LORA
  commands/lora/at_lora.c
  commands/lora/at_lora_p2p.c
  commands/lora/at_lora_test.c
)

if(CONFIG_RZI_AT_COMMAND_LORAWAN)
  zephyr_library_sources(
    commands/lorawan/at_lorawan.c
    commands/lorawan/at_lorawan_key_id.c
    commands/lorawan/at_lorawan_join_send.c
    commands/lorawan/at_lorawan_network_management.c
    commands/lorawan/at_lorawan_supplementary.c
    commands/lorawan/at_lorawan_information.c
    commands/lorawan/at_lorawan_class_b.c
    commands/lorawan/at_lorawan_multicast.c
    commands/lorawan/at_lorawan_certification.c
  )
endif()

zephyr_library_sources_ifdef(
  CONFIG_RZI_AT_ADAPTER_UART
  adapters/at_adapter_uart.c
)
