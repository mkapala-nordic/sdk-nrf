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

#include <zephyr/storage/flash_map.h>

/* Set iBeacon demo advertisement data for the test. */
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
		      0xc8) /* Calibrated RSSI @ 1m */
};

/* Timestamp GPIO */

#define TIMESTAMP_COUNT 2

enum timestamp {
	TIMESTAMP_RRAM_WRITE,
	TIMESTAMP_CPU_SLEEP,
};

#define _TIMESTAMP_GPIO_DT_SPEC_GET(_idx, ...)  GPIO_DT_SPEC_GET(DT_ALIAS(timestamp ## _idx), gpios)

static const struct gpio_dt_spec timestamps[] = {
	LISTIFY(TIMESTAMP_COUNT, _TIMESTAMP_GPIO_DT_SPEC_GET, (,), )
};

static uint64_t timestamp_start[ARRAY_SIZE(timestamps)];

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

	for (size_t i = 0; i < ARRAY_SIZE(timestamps); i++) {
		ret = timestamp_init_gpio(&timestamps[i]);
		if (ret != 0) {
			printk("Cannot initialize timestamp %d GPIO %d!\n", i, ret);
			return ret;
		}
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

static uint64_t uptime_us(void)
{
	uint64_t cycles = k_cycle_get_64();
	return (uint64_t)k_cyc_to_us_near64(cycles);
}

static void timestamp_set(enum timestamp ts, bool set)
{
	__ASSERT(ts < ARRAY_SIZE(timestamps), "Timestamp index out of bounds");

	if (set) {
		timestamp_start[ts] = uptime_us();
		timestamp_set_gpio(&timestamps[ts], true);
	} else {
		timestamp_set_gpio(&timestamps[ts], false);
		printk("Timestamp %d: %lld\n", ts, (uptime_us() - timestamp_start[ts]));
		timestamp_start[ts] = 0;
	}
}

/* Raw flash using flash area API */
#define RAW_FLASH_PARTITION_ID DT_FIXED_PARTITION_ID(DT_NODELABEL(raw_flash_partition))

static int write_flash_area(const uint8_t *data, size_t len)
{
	const struct flash_area *fa;
	int err;

	err = flash_area_open(RAW_FLASH_PARTITION_ID, &fa);
	if (err) {
		printk("Unable to open flash area: %d\n", err);
		return err;
	}

	/* Optional: Erase before write if required by use-case
	 * Here we assume user manages erasure as needed.
	 */

	err = flash_area_write(fa, 0, data, len);
	if (err) {
		printk("Failed to write flash area: %d\n", err);
		flash_area_close(fa);
		return err;
	}

	flash_area_close(fa);
	return 0;
}

static int read_flash_area(uint8_t *data, size_t len)
{
	const struct flash_area *fa;
	int err;

	err = flash_area_open(RAW_FLASH_PARTITION_ID, &fa);
	if (err) {
		printk("Unable to open flash area: %d\n", err);
		return err;
	}

	err = flash_area_read(fa, 0, data, len);
	if (err) {
		printk("Failed to read flash area: %d\n", err);
		flash_area_close(fa);
		return err;
	}

	flash_area_close(fa);
	return 0;
}

static int erase_flash_area(int flash_area_id, size_t size)
{
	const struct flash_area *fa;
	int err;

	err = flash_area_open(flash_area_id, &fa);
	if (err) {
		printk("Unable to open flash area for erase: %d\n", err);
		return err;
	}

	err = flash_area_erase(fa, 0, size);
	if (err) {
		printk("Failed to erase flash area: %d\n", err);
		flash_area_close(fa);
		return err;
	}

	flash_area_close(fa);
	return 0;
}

#define FLASH_BUF_SIZE (10 * 1024)

static uint8_t flash_buf[FLASH_BUF_SIZE];
static uint8_t verify_buf[FLASH_BUF_SIZE];

static void fill_flash_buf(void)
{
	for (size_t i = 0; i < FLASH_BUF_SIZE; i++) {
		flash_buf[i] = i;
	}
}

static void busy_rram(void)
{
	printk("Erasing flash area\n");
	erase_flash_area(RAW_FLASH_PARTITION_ID, FLASH_BUF_SIZE);

	k_msleep(500);

	printk("Writing flash area\n");
	timestamp_set(TIMESTAMP_RRAM_WRITE, true);
	write_flash_area(flash_buf, FLASH_BUF_SIZE);
	timestamp_set(TIMESTAMP_RRAM_WRITE, false);

	k_msleep(500);

	memset(verify_buf, 0, FLASH_BUF_SIZE);
	printk("Reading flash area\n");
	read_flash_area(verify_buf, FLASH_BUF_SIZE);

	printk("Verifying flash area\n");
	if (memcmp(flash_buf, verify_buf, FLASH_BUF_SIZE)) {
		printk("Flash area read not match\n");
	} else {
		printk("Flash area read match\n");
	}
}

static void busy_thread_entry(void *p1, void *p2, void *p3)
{
	printk("Busy thread started\n");

	printk("Filling flash buffer\n");
	fill_flash_buf();

	while (1) {
		printk("==> Busy work start\n");
		busy_rram();
		printk("<== Busy work end\n");

		printk("==> Sleeping\n");
		timestamp_set(TIMESTAMP_CPU_SLEEP, true);
		k_msleep(2000);
		timestamp_set(TIMESTAMP_CPU_SLEEP, false);
		printk("<== Sleeping done\n");
	}
}

static K_THREAD_STACK_DEFINE(busy_thread_stack, 1024);
static struct k_thread busy_thread;

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	/* Start non-connectable advertising with 50 ms advertisement interval. */
	err = bt_le_adv_start(BT_LE_ADV_PARAM(0, 0x0050, 0x0050, NULL),
			      ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("iBeacon advertisement started\n");

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

	printk("Starting RRAM radio sync test\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}
	return 0;
}
