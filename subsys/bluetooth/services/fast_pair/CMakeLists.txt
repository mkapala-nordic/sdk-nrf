#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library_named(fast_pair)

zephyr_library_include_directories(include)
zephyr_library_include_directories(include/common)
# This directory is included to make a workaround of Android issue with sending old RPA address
# during Key-based Pairing write.
zephyr_library_include_directories(${ZEPHYR_BASE}/subsys/bluetooth)

zephyr_library_sources_ifdef(CONFIG_BT_FAST_PAIR_ADVERTISING	   fp_advertising.c)
zephyr_library_sources_ifdef(CONFIG_BT_FAST_PAIR_AUTH		   fp_auth.c)
zephyr_library_sources_ifdef(CONFIG_BT_FAST_PAIR_GATT_SERVICE	   fp_gatt_service.c)
zephyr_library_sources_ifdef(CONFIG_BT_FAST_PAIR_KEYS		   fp_keys.c)
zephyr_library_sources_ifdef(CONFIG_BT_FAST_PAIR_REGISTRATION_DATA fp_registration_data.c)
zephyr_library_sources_ifdef(CONFIG_BT_FAST_PAIR_BATTERY	   fp_battery.c)
zephyr_library_sources_ifdef(CONFIG_BT_FAST_PAIR_STORAGE_INTEGRATION fp_storage_integration.c)

zephyr_linker_sources_ifdef(CONFIG_BT_FAST_PAIR_ACTIVATION SECTIONS fp_activation.ld)
zephyr_library_sources_ifdef(CONFIG_BT_FAST_PAIR_ACTIVATION	    fp_activation.c)

if(CONFIG_BT_FAST_PAIR_CRYPTO)
  add_subdirectory(fp_crypto)
  target_link_libraries(fast_pair PRIVATE fp_crypto)
endif()

if(CONFIG_BT_FAST_PAIR_STORAGE)
  add_subdirectory(fp_storage)
  target_link_libraries(fast_pair PRIVATE fp_storage)
endif()

if(SYSBUILD)
  zephyr_get(FP_DATA_PRESENT MERGE SYSBUILD)

  if(NOT FP_DATA_PRESENT AND TARGET zephyr_interface)
    zephyr_compile_definitions(
      FP_DATA_NOT_PRESENT
      )
  endif()
endif()

if(NOT SYSBUILD)
  set(FP_PARTITION_NAME bt_fast_pair)
  set(
    FP_PROVISIONING_DATA_HEX
    ${CMAKE_BINARY_DIR}/modules/nrf/subsys/bluetooth/services/fast_pair/fp_provisioning_data.hex
    )

  if(NOT DEFINED FP_MODEL_ID OR NOT DEFINED FP_ANTI_SPOOFING_KEY)
    message(FATAL_ERROR "FP_MODEL_ID and FP_ANTI_SPOOFING_KEY build variables must be specified.")
  endif()

  if(CONFIG_PARTITION_MANAGER_ENABLED)
    message(WARNING "CONFIG_PARTITION_MANAGER_ENABLED=y")

    set(FP_PROVISIONING_DATA_ADDRESS $<TARGET_PROPERTY:partition_manager,PM_BT_FAST_PAIR_ADDRESS>)

    add_custom_command(
      OUTPUT
      ${FP_PROVISIONING_DATA_HEX}
      DEPENDS
      "${CMAKE_BINARY_DIR}/pm.config"
      COMMAND
      ${PYTHON_EXECUTABLE} ${ZEPHYR_NRF_MODULE_DIR}/scripts/nrf_provision/fast_pair/fp_provision_cli.py
                          -o ${FP_PROVISIONING_DATA_HEX} -a ${FP_PROVISIONING_DATA_ADDRESS}
                          -m ${FP_MODEL_ID} -k ${FP_ANTI_SPOOFING_KEY}
      COMMENT
      "Generating Fast Pair provisioning data hex file"
      USES_TERMINAL
      )

    add_custom_target(
      ${FP_PARTITION_NAME}_target
      DEPENDS
      "${FP_PROVISIONING_DATA_HEX}"
      )

    set_property(
      GLOBAL PROPERTY
      ${FP_PARTITION_NAME}_PM_HEX_FILE
      "${FP_PROVISIONING_DATA_HEX}"
      )

    set_property(
      GLOBAL PROPERTY
      ${FP_PARTITION_NAME}_PM_TARGET
      ${FP_PARTITION_NAME}_target
      )
  else()
    message(WARNING "CONFIG_PARTITION_MANAGER_ENABLED=n")

    dt_nodelabel(dts_partition_path NODELABEL "bt_fast_pair_partition")
    dt_reg_addr(bt_fast_pair_off PATH ${dts_partition_path})

    set(FP_PROVISIONING_DATA_ADDRESS ${bt_fast_pair_off})

    execute_process(
      COMMAND ${PYTHON_EXECUTABLE} ${ZEPHYR_NRF_MODULE_DIR}/scripts/nrf_provision/fast_pair/fp_provision_cli.py
                          -o ${FP_PROVISIONING_DATA_HEX} -a ${FP_PROVISIONING_DATA_ADDRESS}
                          -m ${FP_MODEL_ID} -k ${FP_ANTI_SPOOFING_KEY}
      COMMAND_ECHO STDOUT
    )

    set_property(
      GLOBAL APPEND PROPERTY
      HEX_FILES_TO_MERGE
      ${FP_PROVISIONING_DATA_HEX}
      ${CMAKE_BINARY_DIR}/zephyr/zephyr.hex
    )

    set_property(
      TARGET
      runners_yaml_props_target
      PROPERTY
      hex_file
      ${CMAKE_BINARY_DIR}/zephyr/merged.hex
    )
  endif()
endif()
