.. _cpu_block_during_radio_test:

CPU block during radio test
###########################

.. contents::
   :local:
   :depth: 2

The test benchmarks the CPU block during radio operation affects the peak current consumption.

Requirements
************

The test supports the following development kit:

.. table-from-sample-yaml::

To observe the peak current consumption, you can use the `Power Profiler Kit II (PPK2)`_ for power profiling and optimizing your configuration.

The sample also toggles the timestamp GPIOs to observe the time of the CPU state and radio operation.

You can connect the timestamp GPIOs to the logic port of the PPK2 kit to synchronize the current measurement with the appropriate CPU state and radio operation.
The following pins are used for the timestamp GPIOs on the nRF54LV10 DK development kit:

* ``GPIO 1.15`` - The timestamp GPIO for the CPU busy operation, connect to the ``D0`` logic channel
* ``GPIO 1.16`` - The timestamp GPIO for the CPU sleep operation, connect to the ``D1`` logic channel
* ``GPIO 1.17`` - The timestamp GPIO for the radio operations (``RADIO->EVENTS_READY`` and ``RADIO->EVENTS_DISABLED``), connect to the ``D2`` logic channel
* ``GPIO 1.18`` - The timestamp GPIO for the radio operations (``RADIO->EVENTS_ADDRESS`` and ``RADIO->EVENTS_END``), connect to the ``D3`` logic channel

You can use also your proprietary solution for measuring the power consumption.

Overview
********

This test demonstrates how the CPU block during radio operation affects the peak current consumption.
The feature blocks all interrupts not related to the radio operations when the radio is active.
The peak current related to the radio operations by default can be over 10mA, which already is at the edge of the peak current budget.
That is why it is important to minimize the simultaneous heavy operations on the CPU when the radio is active.
The downside is that the performance of the CPU is affected and CPU operations take longer to complete.

The test runs the following operations:

1. Initialize the test.
#. Start the iBeacon advertisement with 50 ms advertisement interval.
#. Start a CPU busy thread that:

   a. Simulates CPU busy by doing a heavy floating-point workload.
   #. Sleep for 2 seconds.
   #. Repeats the cycle.


Building and running
********************

.. |test path| replace:: :file:`tests/benchmarks/rram_throttling`

.. include:: /includes/build_and_run_test.txt

Testing
=======

Complete the following steps to test the CPU block during radio operation:

1. Build and program the test with the CPU block during radio operation feature disabled.
#. Connect the Power Profiler Kit II (PPK2) to the development kit and set up for current measurement.
   Connect the timestamp GPIOs to the logic port of the PPK2 kit to synchronize the current measurement with the appropriate CPU state and radio operation.
#. Launch the `Power Profiler app`_ from nRF Connect for Desktop.
#. Reset the kit.
#. Using `nRF Connect for Mobile`_ observe that the development kit is advertising.
   Observe the high current peaks on the Power Profiler app and verify the state changes of the timestamp GPIOs related to the radio operation (``D2`` and ``D3``).
#. Observe that tests periodically runs the simulated CPU busy operation.
   Observe increased current consumption during the CPU busy operation (``D0`` logic channel) and decreased current consumption during the CPU sleep operation (``D1`` logic channel).
#. Measure and write down the peak current consumption during the radio operation (``D2`` and ``D3`` logic channels) when the CPU sleeps (when the ``D1`` logic channel is high).
#. Measure and write down the peak current consumption during the radio operation (``D2`` and ``D3`` logic channels) when the CPU is busy (when the ``D0`` logic channel is high).
#. Repeat the test for the CPU block during radio operation feature enabled.
   Observe how the peak current consumption changes during the radio operations when the CPU is busy compared to when the CPU is sleeping.
