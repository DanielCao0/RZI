# SPDX-License-Identifier: Apache-2.0

if(CONFIG_RZI AND CONFIG_RZI_MCUBOOT AND NOT SYSBUILD)
  message(FATAL_ERROR
    "CONFIG_RZI_MCUBOOT requires sysbuild so MCUboot is built and merged. "
    "Use: west build -b <rzi-board> --sysbuild <app>. "
    "Flash the merged image from the build directory, not zephyr.hex.")
endif()

if(CONFIG_RZI_LORAWAN_BACKEND_USP AND (CONFIG_LORA OR CONFIG_LORAWAN))
  message(FATAL_ERROR
    "CONFIG_RZI_LORAWAN_BACKEND_USP cannot share the radio with "
    "CONFIG_LORA or CONFIG_LORAWAN. Keep both off in prj.conf.")
endif()

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

  if(CONFIG_RZI_SLOT_UPDATE)
    add_subdirectory(
      ${ZEPHYR_CURRENT_MODULE_DIR}/src/slot_update
      ${CMAKE_CURRENT_BINARY_DIR}/rzi-slot-update
    )
  endif()
endif()
