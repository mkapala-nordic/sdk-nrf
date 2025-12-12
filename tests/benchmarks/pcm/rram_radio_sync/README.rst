.. _rram_radio_sync_test:

RRAM radio sync test
####################

.. contents::
   :local:
   :depth: 2

The test benchmarks the RRAM radio sync feature affects the peak current consumption.

Requirements
************

The test supports the following development kit:

.. table-from-sample-yaml::

To observe the peak current consumption, you can use the `Power Profiler Kit II (PPK2)`_ for power profiling and optimizing your configuration.

The sample also toggles the timestamp GPIOs to observe the time of the RRAM write operation, CPU sleep operation, and, radio operation.

You can connect the timestamp GPIOs to the logic port of the PPK2 kit to synchronize the current measurement with the appropriate RRAM write operation, CPU sleep operation, and radio operation.
The following pins are used for the timestamp GPIOs on the nRF54LV10 DK development kit:

* ``GPIO 1.15`` - The timestamp GPIO for the RRAM write operation, connect to the ``D0`` logic channel
* ``GPIO 1.16`` - The timestamp GPIO for the CPU sleep operation, connect to the ``D1`` logic channel
* ``GPIO 1.17`` - The timestamp GPIO for the radio operations (``RADIO->EVENTS_READY`` and ``RADIO->EVENTS_DISABLED``), connect to the ``D2`` logic channel
* ``GPIO 1.18`` - The timestamp GPIO for the radio operations (``RADIO->EVENTS_ADDRESS`` and ``RADIO->EVENTS_END``), connect to the ``D3`` logic channel

You can use also your proprietary solution for measuring the power consumption.

Overview
********

This test demonstrates how the RRAM radio sync feature (:kconfig:option:`CONFIG_SOC_FLASH_NRF_RADIO_SYNC_MPSL`) affects the peak current consumption.
The feature synchronizes the RRAM write operation with the radio operation.
The peak current related to the radio operations by default can be over 10mA and related to the RRAM write operation can be over 14mA, which already is at the edge of the peak current budget.
If both operations are performed simultaneously, the peak current can be way over the budget.
Simultaneous operations can also cause instabilities and timing issues for the radio operation.

The test runs the following operations:

1. Initialize the test.
#. Start the iBeacon advertisement with 50 ms advertisement interval.
#. Start a RRAM write thread that:

   a. Erases the RRAM partition.
   #. Waits for 500 ms.
   #. Writes ``X * 1024`` bytes of data to the RRAM using the Flash Area API.
   #. Waits for 500 ms.
   #. Verifies the RRAM content.
   #. Sleeps for 2 seconds.
   #. Repeats the cycle.


Building and running
********************

.. |test path| replace:: :file:`tests/benchmarks/rram_throttling`

.. include:: /includes/build_and_run_test.txt

Testing
=======

Complete the following steps to test the RRAM radio sync feature:

1. Build and program the test with the CPU block during radio operation feature disabled.
#. Connect the Power Profiler Kit II (PPK2) to the development kit and set up for current measurement.
   Connect the timestamp GPIOs to the logic port of the PPK2 kit to synchronize the current measurement with the appropriate CPU state and radio operation.
#. Launch the `Power Profiler app`_ from nRF Connect for Desktop.
#. Reset the kit.
#. Using `nRF Connect for Mobile`_ observe that the development kit is advertising.
   Observe the high current peaks on the Power Profiler app and verify the state changes of the timestamp GPIOs related to the radio operation (``D2`` and ``D3``).
#. Observe that test periodically runs the RRAM write operation.
   Observe increased current consumption during the RRAM write operation (``D0`` logic channel).
#. When the synchronization is enabled, observe that the RRAM write (``D0``) is interrupted temporarily to allow the radio operation (``D2`` and ``D3``) to perform.
   Observe that RRAM write operation resumes after the radio operation is finished.
#. When the synchronization is disabled, observe that the RRAM write operation is not interrupted by the radio operation and both operations are performed at the same time.
   You might observe the asserts related to radio operations or unstable radio operation.
