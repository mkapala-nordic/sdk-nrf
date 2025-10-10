.. _rram_throttling_test:

RRAM throttling test
####################

.. contents::
   :local:
   :depth: 2

The test benchmarks the RRAM throttling feature affects the peak current consumption.

Requirements
************

The test supports the following development kit:

.. table-from-sample-yaml::

To observe the peak current consumption, you can use the `Power Profiler Kit II (PPK2)`_ for power profiling and optimizing your configuration.

The sample also toggles the timestamp GPIOs to observe the time of the RRAM write operation.

You can connect the timestamp GPIOs to the logic port of the PPK2 kit to synchronize the current measurement with the appropriate RRAM operation.
The following pins are used for the timestamp GPIOs on the nRF54LV10 DK development kit:

* ``GPIO 1.15`` - The timestamp GPIO for the RRAM write operation, connect to the ``D0`` logic channel
* ``GPIO 1.16`` - The timestamp GPIO for the RRAM read operation, connect to the ``D1`` logic channel
* ``GPIO 1.17`` - The timestamp GPIO for the RRAM erase operation, connect to the ``D2`` logic channel
* ``GPIO 1.18`` - The timestamp GPIO for the test duration, connect to the ``D3`` logic channel

You can use also your proprietary solution for measuring the power consumption.

Overview
********

This test demonstrates how the RRAM throttling feature affects the peak current consumption when writing to the RRAM.
This feature splits the RRAM write operation into multiple write operations of smaller size with the delay between each operation.
Without the RRAM throttling feature, the peak current consumption can spike to 14mA or more (measured for the nRF54LV10 DK).
With the RRAM throttling feature, the peak current consumption can be reduced to 10mA or less, but the operation time is increased.

The test runs the following operations:

1. Initialize the test.
#. Erase the RRAM partitions.
#. Wait 2 seconds.
#. Start writing to the RRAM a data block of 1024 bytes using the Flash Area API.
#. Wait 2 seconds.
#. Start writing to the RRAM a data block of 1024 bytes using the ZMS API.

You can configure the RRAM throttling feature by changing the Kconfig options:

* :kconfig:option:`CONFIG_SOC_FLASH_NRF_THROTTLING` - Enable RRAM throttling
* :kconfig:option:`CONFIG_NRF_RRAM_THROTTLING_DATA_BLOCK` - Set the data block size in 128-bit words (default value is 16 blocks)
* :kconfig:option:`CONFIG_NRF_RRAM_THROTTLING_DELAY` - Set the delay between the write operations in microseconds (default value is 2000 us)

Building and running
********************

.. |test path| replace:: :file:`tests/benchmarks/rram_throttling`

.. include:: /includes/build_and_run_test.txt

Use the following file suffixes (for the ``-DFILE_SUFFIX`` option) to build the test:

* no file suffix - Disable RRAM throttling (default configuration)
* ``on_8blocks`` file suffix - Enable RRAM throttling with 8 blocks
* ``on_16blocks`` file suffix - Enable RRAM throttling with 16 blocks (default configuration after enabling the RRAM throttling feature)

To build the test using the file suffix, use the following command:

.. code-block:: console

   west build -b nrf54lv10dk/nrf54lv10a/cpuapp -- -DFILE_SUFFIX=<file_suffix>

Testing
=======

Complete the following steps to test the RRAM throttling feature:

1. Build and program the test with the disabled RRAM throttling feature (no file suffix).
#. Connect the Power Profiler Kit II (PPK2) to the development kit and set up for current measurement.
   Connect the timestamp GPIOs to the logic port of the PPK2 kit to synchronize the current measurement with the appropriate RRAM operation.
#. Launch the `Power Profiler app`_ from nRF Connect for Desktop.
#. Reset the kit.
#. The test initializes and erases the RRAM partitions.
   Observe the high pin state on the ``D2`` logic channel during the RRAM erase operation and high current consumption.
#. Wait 2 seconds.
#. The test starts writing to the RRAM a data block of 1024 bytes using the Flash Area API.
   Observe the high pin state on the ``D0`` logic channel during the RRAM write operation.
   Observe that after ``D0`` logic channel goes low, the high pin state on the ``D1`` logic channel during the RRAM read operation briefly goes high.
   Observe high current consumption during the RRAM operations and write down the peak current consumption.
#. Wait 2 seconds.
#. The test starts writing to the RRAM a data block of 1024 bytes using the ZMS API.
   Observe the high pin state on the ``D0`` logic channel during the RRAM write operation.
   Observe that after ``D0`` logic channel goes low, the high pin state on the ``D1`` logic channel during the RRAM read operation briefly goes high.
   Observe high current consumption during the RRAM operations and write down the peak current consumption.
#. Repeat the test after changing the RRAM throttling feature to enabled with 8 blocks (file suffix ``on_8blocks``).
   Observe how the peak current consumption changes.
#. Repeat the test after changing the RRAM throttling feature to enabled with 16 blocks (file suffix ``on_16blocks``).
   Observe how the peak current consumption changes.
