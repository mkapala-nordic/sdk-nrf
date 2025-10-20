/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/drivers/gpio.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

#ifndef IBEACON_RSSI
#define IBEACON_RSSI 0xc8
#endif

/*
 * Set iBeacon demo advertisement data. These values are for
 * demonstration only and must be changed for production environments!
 *
 * UUID:  18ee1516-016b-4bec-ad96-bcb96d166e97
 * Major: 0
 * Minor: 0
 * RSSI:  -56 dBm
 */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA,
		      0x4c, 0x00, /* Apple */
		      0x02, 0x15, /* iBeacon */
		      0x18, 0xee, 0x15, 0x16, /* UUID[15..12] */
		      0x01, 0x6b, /* UUID[11..10] */
		      0x4b, 0xec, /* UUID[9..8] */
		      0xad, 0x96, /* UUID[7..6] */
		      0xbc, 0xb9, 0x6d, 0x16, 0x6e, 0x97, /* UUID[5..0] */
		      0x00, 0x00, /* Major */
		      0x00, 0x00, /* Minor */
		      IBEACON_RSSI) /* Calibrated RSSI @ 1m */
};

/* Timestamp GPIO */

static uint64_t uptime_us(void)
{
	uint64_t cycles = k_cycle_get_64();
	return (uint64_t)k_cyc_to_us_near64(cycles);
}

static const struct gpio_dt_spec timestamp0 = GPIO_DT_SPEC_GET(DT_ALIAS(timestamp0), gpios);
static const struct gpio_dt_spec timestamp1 = GPIO_DT_SPEC_GET(DT_ALIAS(timestamp1), gpios);

static int timestamp_init_gpio(const struct gpio_dt_spec *gpio) {
	int ret;

	if (!device_is_ready(gpio->port)) {
		printk("Cannot initialize \"%s\"!\n", gpio->port->name);
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(gpio, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		printk("Cannot initialize timestamp GPIO \"%s\" (ret %d)!\n", gpio->port->name, ret);
		return ret;
	}

	ret = gpio_pin_set_dt(gpio, 0);
	if (ret != 0) {
		printk("Cannot set timestamp GPIO \"%s\" (ret %d)!\n", gpio->port->name, ret);
		return ret;
	}

	return 0;
}

static int timestamp_init(void)
{
	int ret;

	ret = timestamp_init_gpio(&timestamp0);
	if (ret != 0) {
		printk("Cannot initialize timestamp0 GPIO %d!\n", ret);
		return ret;
	}

	ret = timestamp_init_gpio(&timestamp1);
	if (ret != 0) {
		printk("Cannot initialize timestamp1 GPIO %d!\n", ret);
		return ret;
	}

	return 0;
}

static void timestamp_set_gpio(const struct gpio_dt_spec *gpio, bool set)
{
	int ret = gpio_pin_set_dt(gpio, set ? 1 : 0);
	if (ret != 0) {
		printk("Cannot set timestamp GPIO \"%s\" (ret %d)!\n", gpio->port->name, ret);
	}
}

static void timestamp0_set(bool set)
{
	static uint64_t timestamp;

	if (set) {
		timestamp = uptime_us();
		timestamp_set_gpio(&timestamp0, true);
	} else {
		timestamp_set_gpio(&timestamp0, false);
		printk("Timestamp0: %lld\n", (uptime_us() - timestamp));
		timestamp = 0;
	}
}

static void timestamp1_set(bool set)
{
	static uint64_t timestamp;

	if (set) {
		timestamp = uptime_us();
		timestamp_set_gpio(&timestamp1, true);
	} else {
		timestamp_set_gpio(&timestamp1, false);
		printk("Timestamp1: %lld\n", (uptime_us() - timestamp));
		timestamp = 0;
	}
}

void busy_cpu_math(void)
{
	static float a = 1.01f;
	static float b = 1.001f;
	static float c = 0.0f;

	// Heavy floating-point workload
	for (int i = 0; i < 100000; i++) {
		c += a * b;
		if (c > 10000.0f) {
			c = 0.0f;
		}
	}
}

static void busy_thread_entry(void *p1, void *p2, void *p3)
{
	printk("Busy thread started\n");

	while (1) {
		printk("==> Busy work start\n");
		timestamp0_set(true);

		/* SIMULATE CPU LOAD */
		busy_cpu_math();

		timestamp0_set(false);
		printk("<== Busy work end\n");

		////////////////////////////////////////

		printk("==> Sleeping\n");
		timestamp1_set(true);

		k_msleep(2000);
		// k_yield();

		timestamp1_set(false);
		printk("<== Sleeping done\n");
	}

	// while (1) {
	// 	k_msleep(1000);
	// }
}

static k_tid_t busy_thread_id;
static K_THREAD_STACK_DEFINE(busy_thread_stack, 1024);
static struct k_thread busy_thread;

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	/* Start advertising */
	err = bt_le_adv_start(BT_LE_ADV_PARAM(0,
					      0x0050, // 50 ms
					      0x0050, // 50 ms
					      NULL),
			      ad, ARRAY_SIZE(ad),
			      NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("iBeacon started\n");

	/* Start CPU load thread */
	k_thread_create(&busy_thread,
		busy_thread_stack,
		1024,
		busy_thread_entry,
		NULL, NULL, NULL,
		K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
}

int main(void)
{
	int err;

	err = timestamp_init();
	if (err) {
		printk("timestamp_init failed (err %d)\n", err);
		return err;
	}

	printk("Starting iBeacon Demo\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}
	return 0;
}
