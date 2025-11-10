/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

#include "../../../../../samples/radio_loader/shared_magic/shared_magic.h"

LOG_MODULE_REGISTER(idle);

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

	/* Copy the firmware to the TCM. */
	memcpy((void *)LOADED_FW_RAM_ADDR, (void *)LOADED_FW_ADDR, LOADED_FW_SIZE);
	printk("Copied the firmware to the TCM.\n");

	/* Set the magic to the boot ready magic. */
	shared_magic_set_boot_ready();
	printk("Magic set to the boot ready magic.\n");
}

int main(void)
{
	unsigned int cnt = 0;

	if (IS_ENABLED(CONFIG_RADIO_LOADER_MODE_MPC)) {
		radio_fw_copy();
	}

	LOG_INF("Multicore idle test on %s", CONFIG_BOARD_TARGET);
	while (1) {
		LOG_INF("Multicore idle test iteration %u", cnt++);
		k_msleep(2000);
	}

	return 0;
}
