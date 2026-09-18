# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(lora.c)

zephyr_library_sources_ifdef(
  CONFIG_RZI_LORA_BACKEND_TEST
  backend/lora_backend_test.c
)
zephyr_library_sources_ifdef(
  CONFIG_RZI_LORA_BACKEND_ZEPHYR
  backend/lora_backend_zephyr.c
)
zephyr_library_sources_ifdef(
  CONFIG_RZI_LORA_BACKEND_USP
  backend/lora_backend_usp.c
)
zephyr_library_link_libraries_ifdef(
  CONFIG_RZI_LORA_BACKEND_USP
  lbm_compile_definitions
)
