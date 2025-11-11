/*
 * Copyright (c) 2018 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ibeacon);

#ifndef IBEACON_RSSI
#define IBEACON_RSSI 0xc8
#endif

#define ADV_INT(ms) ((uint16_t)((ms) / 0.625))
#define BT_LE_ADV_PARAM_20MS_INTERVAL BT_LE_ADV_PARAM(0, ADV_INT(20), ADV_INT(20), NULL)
#define BT_LE_ADV_CONN_PARAM_100MS_INT BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN, ADV_INT(100), ADV_INT(100), NULL)

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

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	/* Start advertising */
	err = bt_le_adv_start(BT_LE_ADV_CONN_PARAM_100MS_INT, ad, ARRAY_SIZE(ad),
			      NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("iBeacon started\n");
}

#include "../../../../../samples/radio_loader/shared_magic/shared_magic.h"

#define LOADED_FW_ADDR        		0x0e0a9000
#define LOADED_FW_SIZE        		0x20000

#define LOADED_FW_RAM_ADDR    		0x23000000
#define LOADED_FW_RAM_SIZE    		0x20000

void radio_fw_copy(void)
{
	printk("Waiting for the MPC to be configured.\n");
	/* Wait for the MPC to be configured. */
	shared_magic_wait_for_mpc_ready();
	printk("MPC is configured.\n");

	/* Consume the magic. */
	shared_magic_consume();

	//todo: check which cache ops are needed and where
	sys_cache_data_flush_and_invd_all();

	/* Copy the firmware to the TCM. */
	memcpy((void *)LOADED_FW_RAM_ADDR, (void *)LOADED_FW_ADDR, LOADED_FW_SIZE);
	printk("Copied the firmware to the TCM.\n");

	//todo: check which cache ops are needed and where
	sys_cache_data_flush_and_invd_all();

	/* Set the magic to the boot ready magic. */
	shared_magic_set_boot_ready();
	printk("Magic set to the boot ready magic.\n");
}

int main(void)
{
	int err;

	uintptr_t pc;
	__asm__ volatile("adr %0, ." : "=r"(pc));
	printk("Current PC (program counter) address: 0x%lx\n", (unsigned long)pc);

	if (IS_ENABLED(CONFIG_RADIO_LOADER_MODE_MPC)) {
		radio_fw_copy();
	}

	printk("Starting iBeacon Demo\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}
	return 0;
}
