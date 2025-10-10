/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <zephyr/fs/zms.h>
#include <zephyr/storage/flash_map.h>

#include <zephyr/drivers/gpio.h>

/* Timestamp GPIO */

static uint64_t uptime_us(void)
{
	uint64_t cycles = k_cycle_get_64();
	return (uint64_t)k_cyc_to_us_near64(cycles);
}

static const struct gpio_dt_spec timestamp0 = GPIO_DT_SPEC_GET(DT_ALIAS(timestamp0), gpios);
static const struct gpio_dt_spec timestamp1 = GPIO_DT_SPEC_GET(DT_ALIAS(timestamp1), gpios);
static const struct gpio_dt_spec timestamp2 = GPIO_DT_SPEC_GET(DT_ALIAS(timestamp2), gpios);
static const struct gpio_dt_spec timestamp3 = GPIO_DT_SPEC_GET(DT_ALIAS(timestamp3), gpios);

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

	ret = timestamp_init_gpio(&timestamp2);
	if (ret != 0) {
		printk("Cannot initialize timestamp2 GPIO %d!\n", ret);
		return ret;
	}

	ret = timestamp_init_gpio(&timestamp3);
	if (ret != 0) {
		printk("Cannot initialize timestamp3 GPIO %d!\n", ret);
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

static void timestamp2_set(bool set)
{
	static uint64_t timestamp;

	if (set) {
		timestamp = uptime_us();
		timestamp_set_gpio(&timestamp2, true);
	} else {
		timestamp_set_gpio(&timestamp2, false);
		printk("Timestamp2: %lld\n", (uptime_us() - timestamp));
		timestamp = 0;
	}
}

static void timestamp3_set(bool set)
{
	static uint64_t timestamp;

	if (set) {
		timestamp = uptime_us();
		timestamp_set_gpio(&timestamp3, true);
	}
	else {
		timestamp_set_gpio(&timestamp3, false);
		printk("Timestamp3: %lld\n", (uptime_us() - timestamp));
		timestamp = 0;
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

	timestamp0_set(true);
	err = flash_area_write(fa, 0, data, len);
	timestamp0_set(false);
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

	timestamp1_set(true);
	err = flash_area_read(fa, 0, data, len);
	timestamp1_set(false);
	if (err) {
		printk("Failed to read flash area: %d\n", err);
		flash_area_close(fa);
		return err;
	}

	flash_area_close(fa);
	return 0;
}

static int erase_flash_area(int flash_area_id)
{
	const struct flash_area *fa;
	int err;

	err = flash_area_open(flash_area_id, &fa);
	if (err) {
		printk("Unable to open flash area for erase: %d\n", err);
		return err;
	}

	timestamp2_set(true);
	err = flash_area_erase(fa, 0, fa->fa_size);
	timestamp2_set(false);
	if (err) {
		printk("Failed to erase flash area: %d\n", err);
		flash_area_close(fa);
		return err;
	}

	flash_area_close(fa);
	return 0;
}


/* ZMS filesystem */
#define RAM_ZMS_NODE raw_zms_partition
#define RAW_ZMS_PARTITION_ID DT_FIXED_PARTITION_ID(DT_NODELABEL(RAM_ZMS_NODE))
#define RAW_ZMS_DEVICE FIXED_PARTITION_DEVICE(RAM_ZMS_NODE)
#define RAW_ZMS_STORAGE_OFFSET FIXED_PARTITION_OFFSET(RAM_ZMS_NODE)
#define RAW_ZMS_SECTOR_SIZE  (DT_PROP(DT_CHOSEN(zephyr_flash), erase_block_size))
#define RAW_ZMS_SECTOR_COUNT 2

static struct zms_fs zms_fs = {
	.flash_device = RAW_ZMS_DEVICE,
	.offset = RAW_ZMS_STORAGE_OFFSET,
	.sector_size = RAW_ZMS_SECTOR_SIZE,
	.sector_count = RAW_ZMS_SECTOR_COUNT,
};

#define ZMS_ENTRY_DATA_ID 1

static int write_zms_entry(const uint8_t *data, size_t len)
{
	int ret;

	timestamp0_set(true);
	ret = zms_write(&zms_fs, ZMS_ENTRY_DATA_ID, data, len);
	timestamp0_set(false);
	if (ret != len) {
		printk("Cannot write ZMS entry %d!\n", ret);
		return ret;
	}

	return 0;
}

static int read_zms_entry(uint8_t *data, size_t len)
{
	int ret;

	timestamp1_set(true);
	ret = zms_read(&zms_fs, ZMS_ENTRY_DATA_ID, data, len);
	timestamp1_set(false);
	if (ret != len) {
		printk("Cannot read ZMS entry %d!\n", ret);
		return ret;
	}

	return 0;
}

static int init(void)
{
	int err;

	err = timestamp_init();
	if (err) {
		printk("Cannot initialize timestamp GPIO %d!\n", err);
		return err;
	}

	printk("Erasing flash areas\n");

	err = erase_flash_area(RAW_FLASH_PARTITION_ID);
	if (err) {
		printk("Cannot erase flash area %d!\n", err);
		return err;
	}

	err = erase_flash_area(RAW_ZMS_PARTITION_ID);
	if (err) {
		printk("Cannot erase flash area %d!\n", err);
		return err;
	}

	printk("Initializing ZMS filesystem\n");

	err = zms_mount(&zms_fs);
	if (err) {
		printk("Cannot initialize ZMS filesystem %d!\n", err);
		return err;
	}

	printk("ZMS filesystem initialized\n");

	return err;
}

#define FLASH_BUF_SIZE 1024

static uint8_t flash_buf[FLASH_BUF_SIZE];
static uint8_t verify_buf[FLASH_BUF_SIZE];

static void fill_flash_buf(void)
{
	for (size_t i = 0; i < FLASH_BUF_SIZE; i++) {
		flash_buf[i] = i;
	}
}

int main(void)
{
	int err;

	k_sleep(K_SECONDS(5));

	printk("Starting RRAM Throttling Benchmark\n");

	timestamp3_set(true);

	err = init();
	if (err) {
		printk("Cannot initialize board %d!\n", err);
		return err;
	}

	k_sleep(K_SECONDS(2));

	printk("=== Starting flash area benchmark ===\n");

	printk("Filling flash buffer\n");
	fill_flash_buf();

	printk("Writing flash area\n");
	write_flash_area(flash_buf, FLASH_BUF_SIZE);

	memset(verify_buf, 0, FLASH_BUF_SIZE);
	printk("Reading flash area\n");
	read_flash_area(verify_buf, FLASH_BUF_SIZE);

	printk("Verifying flash area\n");
	if (memcmp(flash_buf, verify_buf, FLASH_BUF_SIZE)) {
		printk("Flash area read not match\n");
	} else {
		printk("Flash area read match\n");
	}

	printk("=== Ending flash area benchmark ===\n");

	k_sleep(K_SECONDS(2));

	printk("=== Starting ZMS benchmark ===\n");

	printk("Writing ZMS entry\n");
	write_zms_entry(flash_buf, FLASH_BUF_SIZE);

	memset(verify_buf, 0, FLASH_BUF_SIZE);
	printk("Reading ZMS entry\n");
	read_zms_entry(verify_buf, FLASH_BUF_SIZE);

	printk("Verifying ZMS entry\n");
	if (memcmp(flash_buf, verify_buf, FLASH_BUF_SIZE)) {
		printk("ZMS entry read not match\n");
	} else {
		printk("ZMS entry read match\n");
	}

	printk("=== Ending ZMS benchmark ===\n");

	timestamp3_set(false);

	return 0;
}
