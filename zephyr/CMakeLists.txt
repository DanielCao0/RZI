# SPDX-License-Identifier: Apache-2.0
if(CONFIG_RZI)
  zephyr_library()

  zephyr_include_directories(${ZEPHYR_CURRENT_MODULE_DIR}/include)

  add_subdirectory(
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/core
    ${CMAKE_CURRENT_BINARY_DIR}/rzi-core
  )

  if(CONFIG_RZI_STORAGE)
    add_subdirectory(
      ${ZEPHYR_CURRENT_MODULE_DIR}/src/storage
      ${CMAKE_CURRENT_BINARY_DIR}/rzi-storage
    )
  endif()

  if(CONFIG_RZI_LORAWAN)
    add_subdirectory(
      ${ZEPHYR_CURRENT_MODULE_DIR}/src/lorawan
      ${CMAKE_CURRENT_BINARY_DIR}/rzi-lorawan
    )
  endif()

  if(CONFIG_RZI_LORA)
    add_subdirectory(
      ${ZEPHYR_CURRENT_MODULE_DIR}/src/lora
      ${CMAKE_CURRENT_BINARY_DIR}/rzi-lora
    )
  endif()

  if(CONFIG_RZI_AT)
    add_subdirectory(
      ${ZEPHYR_CURRENT_MODULE_DIR}/src/at
      ${CMAKE_CURRENT_BINARY_DIR}/rzi-at
    )
  endif()
endif()
