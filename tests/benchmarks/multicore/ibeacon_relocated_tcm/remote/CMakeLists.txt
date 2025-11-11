#
# Copyright (c) 2023 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ipc_radio)

target_include_directories(app PRIVATE ./src)

target_sources(app PRIVATE src/bt_hci_ipc.c)

# NORDIC SDK APP START
target_sources(app PRIVATE
  src/main.c
)
# NORDIC SDK APP END

# Get the relocation address from devicetree chosen node
dt_chosen(loaded_fw_node PROPERTY "fw-to-relocate")
if(DEFINED loaded_fw_node)
    # Get the partition offset from devicetree
    dt_reg_addr(partition_offset PATH ${loaded_fw_node})

    # NOTE: MRAM base address is hardcoded (0xe000000 for nrf54h20)
    # This is the base address of mram0 controller at /soc/mram@e000000
    # If targeting a different SoC, update this value accordingly
    set(mram_base_addr "0xe000000")

    # Calculate absolute address: MRAM base + partition offset
    math(EXPR reloc_addr "${mram_base_addr} + ${partition_offset}" OUTPUT_FORMAT HEXADECIMAL)

    message(STATUS "MRAM base address (hardcoded): ${mram_base_addr}")
    message(STATUS "Partition offset from DTS: ${partition_offset}")
    message(STATUS "Relocation address: ${reloc_addr}")

    # Add custom target to create relocated hex file after binaries are built
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/zephyr/zephyr_relocated.hex
        COMMAND ${CMAKE_OBJCOPY} --input-target=binary --output-target=ihex --change-addresses ${reloc_addr}
                ${CMAKE_BINARY_DIR}/zephyr/zephyr.bin
                ${CMAKE_BINARY_DIR}/zephyr/zephyr_relocated.hex
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_BINARY_DIR}/zephyr/zephyr_relocated.hex
                ${CMAKE_BINARY_DIR}/zephyr/zephyr.hex
        DEPENDS ${CMAKE_BINARY_DIR}/zephyr/zephyr.bin
        COMMENT "Creating relocated hex file at ${reloc_addr} and overwriting zephyr.hex"
        VERBATIM
    )

    add_custom_target(relocated_hex ALL
        DEPENDS ${CMAKE_BINARY_DIR}/zephyr/zephyr_relocated.hex
    )
else()
    message(WARNING "No 'fw-to-relocate' chosen node found in devicetree, skipping relocation")
endif()
