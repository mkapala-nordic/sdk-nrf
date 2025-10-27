.. _multicore_idle_test:

Multicore idle test with firmware relocated to radio core TCM 
#############################################################

.. contents::
   :local:
   :depth: 2

The test benchmarks the idle behavior of an application that runs on multiple cores.
It demonstrates a radio loader pattern where the radio core firmware is loaded from MRAM into TCM (Tightly Coupled Memory) at runtime.

Requirements
************

The test supports the following development kit:

.. table-from-rows:: /includes/sample_board_rows.txt
   :header: heading
   :rows: nrf54h20dk_nrf54h20_cpuapp

Overview
********

This test demonstrates how to build a multicore idle application with :ref:`configuration_system_overview_sysbuild` using a two-stage boot process for the radio core:

1. **Radio Loader** - A small bootloader that runs on the radio core, copies firmware from MRAM to TCM, and jumps to it
2. **Remote Firmware** - The actual application that runs from TCM after being loaded

The test automatically relocates the remote firmware binary to the correct MRAM address during build time, ensuring it can be loaded by the radio loader.

Architecture
============

The system uses the following memory layout:

* **MRAM (Non-volatile):**
  
  * ``cpurad_loader_partition`` @ 0x0e092000 - Contains the radio loader (92 KB)
  * ``cpurad_loaded_fw`` @ 0x0e0a9000 - Contains the remote firmware binary (128 KB)

* **TCM (Volatile, fast execution):**
  
  * ``cpurad_ram0`` @ 0x23000000 - Code execution region (128 KB)
  * ``cpurad_data_ram`` @ 0x23020000 - Data region (64 KB)

Additional Files
================

The test comes with the following additional files:

* :file:`sysbuild.cmake` - Adds additional images using the :c:macro:`ExternalZephyrProject_Add` macro
* :file:`boards/memory_map.overlay` - Shared memory map configuration for both loader and remote firmware
* :file:`remote/CMakeLists.txt` - Contains automatic relocation logic for the remote firmware

Configuring the Radio Loader
*****************************

The radio loader is responsible for copying the remote firmware from MRAM to TCM and jumping to it. 
To configure the radio loader, you need to define the memory map in devicetree overlays.

Memory Map Configuration
========================

The memory map is defined in :file:`boards/memory_map.overlay` and is shared between the radio loader and remote firmware to ensure consistency.

The overlay defines:

1. **TCM regions:**

   .. code-block:: devicetree

      cpurad_ram0: sram@23000000 {
          compatible = "mmio-sram";
          reg = <0x23000000 0x20000>;  /* 128 KB for code */
      };

      cpurad_data_ram: sram@23020000 {
          compatible = "mmio-sram";
          reg = <0x23020000 0x10000>;  /* 64 KB for data */
      };

2. **MRAM partitions:**

   .. code-block:: devicetree

      &{/soc/mram@e000000/partitions} {
          cpurad_loader_partition: partition@92000 {
              label = "cpurad_loader_partition";
              reg = <0x92000 DT_SIZE_K(92)>;
          };

          cpurad_loaded_fw: partition@a9000 {
              label = "cpurad_loaded_fw";
              reg = <0xa9000 0x20000>;  /* 128 KB */
          };
      };

Chosen Nodes
============

Each image (radio loader and remote firmware) requires specific ``chosen`` nodes in its overlay:

**Radio Loader** (:file:`sysbuild/radio_loader/boards/nrf54h20dk_nrf54h20_cpurad.overlay`):

.. code-block:: devicetree

   /{
       chosen {
           zephyr,code-partition = &cpurad_loader_partition;
           zephyr,sram = &cpurad_data_ram;
           zephyr,loaded-fw-src = &cpurad_loaded_fw;  /* Source in MRAM */
           zephyr,loaded-fw-dst = &cpurad_ram0;       /* Destination in TCM */
       };
   };

**Remote Firmware** (:file:`remote/boards/nrf54h20dk_nrf54h20_cpurad.overlay`):

.. code-block:: devicetree

   /{
       chosen {
           zephyr,code-partition = &cpurad_ram0;
           zephyr,sram = &cpurad_data_ram;
           fw-to-relocate = &cpurad_loaded_fw;  /* Used for automatic relocation */
       };
   };

Automatic Firmware Relocation
******************************

The remote firmware must be relocated to match the MRAM partition address where it will be stored.
This is done automatically during the build process using a custom CMake configuration in :file:`remote/CMakeLists.txt`.

How It Works
============

1. **Extract address from devicetree:**
   
   The build system reads the ``fw-to-relocate`` chosen node to find the target partition:

   .. code-block:: cmake

      dt_chosen(loaded_fw_node PROPERTY "fw-to-relocate")
      dt_reg_addr(partition_offset PATH ${loaded_fw_node})

2. **Calculate absolute address:**
   
   The partition offset is relative to the MRAM controller base address (0xe000000):

   .. code-block:: cmake

      set(mram_base_addr "0xe000000")
      math(EXPR reloc_addr "${mram_base_addr} + ${partition_offset}" OUTPUT_FORMAT HEXADECIMAL)

3. **Create relocated binary:**
   
   After the build, ``objcopy`` relocates the binary and overwrites ``zephyr.hex``:

   .. code-block:: cmake

      add_custom_command(
          OUTPUT ${CMAKE_BINARY_DIR}/zephyr/zephyr_relocated.hex
          COMMAND ${CMAKE_OBJCOPY} --input-target=binary --output-target=ihex 
                  --change-addresses ${reloc_addr}
                  ${CMAKE_BINARY_DIR}/zephyr/zephyr.bin 
                  ${CMAKE_BINARY_DIR}/zephyr/zephyr_relocated.hex
          COMMAND ${CMAKE_COMMAND} -E copy 
                  ${CMAKE_BINARY_DIR}/zephyr/zephyr_relocated.hex 
                  ${CMAKE_BINARY_DIR}/zephyr/zephyr.hex
          DEPENDS ${CMAKE_BINARY_DIR}/zephyr/zephyr.bin
      )

The relocated hex file (``zephyr_relocated.hex``) is kept for reference, while ``zephyr.hex`` is overwritten to ensure standard flashing commands work without modification.

Adapting to Your Project
=========================

To use this relocation mechanism in your own project:

1. **Define memory map:**
   
   Create a :file:`memory_map.overlay` file with your partition layout.

2. **Add chosen node:**
   
   Add ``fw-to-relocate = &your_partition;`` to your remote firmware's overlay.

3. **Copy CMake logic:**
   
   Add the relocation logic from :file:`remote/CMakeLists.txt` to your remote image's CMakeLists.txt.

4. **Update MRAM base address:**
   
   If targeting a different SoC, update the hardcoded ``mram_base_addr`` value.

.. note::

   The MRAM base address (0xe000000) is hardcoded for nRF54H20. If you're using a different SoC, update this value in the CMakeLists.txt.

Building and running
********************

.. |test path| replace:: :file:`tests/benchmarks/multicore/idle_relocated_tcm`

.. include:: /includes/build_and_run_test.txt

Build the test for application and radio cores as follows:

.. code-block:: console

   west build -p -b nrf54h20dk/nrf54h20/cpuapp -T benchmarks.multicore.idle.nrf54h20dk_cpuapp_cpurad .

During the build, you will see messages indicating the relocation process:

.. code-block:: console

   -- MRAM base address (hardcoded): 0xe000000
   -- Partition offset from DTS: 0xa9000
   -- Relocation address: 0x0e0a9000

.. include:: /includes/nRF54H20_erase_UICR.txt

Testing
=======

After programming the test to your development kit, complete the following steps to test it:

1. |connect_terminal|
#. Reset the kit.
#. Observe the console output for both cores:

   * For the application core, the output should be as follows:

     .. code-block:: console

       *** Booting nRF Connect SDK zephyr-v3.5.0-3517-g9458a1aaf744 ***
       Multi-core idle test on nrf54h20dk/nrf54h20/cpuapp
       Multi-core idle test iteration 0
       Multi-core idle test iteration 1
       ...

   * For the radio core, the output should be as follows:

     .. code-block:: console

        *** Booting nRF Connect SDK zephyr-v3.5.0-3517-g9458a1aaf744 ***
     Multicore idle test on nrf54h20dk@0.9.0/cpuapp
     Current PC (program counter) address: 0x23000ab8
     Multicore idle test iteration 0
     Multicore idle test iteration 1
     ...

The radio loader first loads the firmware from MRAM (``0x0e0a9000``) to TCM (``0x23000000``) and then jumps to the loaded firmware.
This process is transparent and happens during the early boot stage.
