/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

 #include <zephyr/kernel.h>

#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

#include "shared_magic.h"

LOG_MODULE_REGISTER(radio_loader);

#define LOADED_FW_NODE        		DT_CHOSEN(zephyr_loaded_fw_src)
#define LOADED_FW_PARTITION_NODE 	DT_PARENT(DT_PARENT(LOADED_FW_NODE))
#define LOADED_FW_ADDR        		(DT_REG_ADDR(LOADED_FW_NODE) + DT_REG_ADDR(LOADED_FW_PARTITION_NODE))
#define LOADED_FW_SIZE        		DT_REG_SIZE(LOADED_FW_NODE)

#define LOADED_FW_RAM_NODE    		DT_CHOSEN(zephyr_loaded_fw_dst)
#define LOADED_FW_RAM_ADDR    		DT_REG_ADDR(LOADED_FW_RAM_NODE)
#define LOADED_FW_RAM_SIZE    		DT_REG_SIZE(LOADED_FW_RAM_NODE)

#define NRF_RADIO_MPC ((NRF_MPC_Type*)0x53001000)

void radio_tcm_mpc_configure(void)
{
	NRF_RADIO_MPC->OVERRIDE[4].CONFIG = 2 | 1 << 9 | 1<<12;
	NRF_RADIO_MPC->OVERRIDE[4].STARTADDR = LOADED_FW_RAM_ADDR;
	NRF_RADIO_MPC->OVERRIDE[4].ENDADDR = (LOADED_FW_RAM_ADDR + LOADED_FW_RAM_SIZE);
	NRF_RADIO_MPC->OVERRIDE[4].PERM = 0xF;
	NRF_RADIO_MPC->OVERRIDE[4].PERMMASK = 0xF;
	NRF_RADIO_MPC->OVERRIDE[4].OWNER = 2;
}

 static int wait_loop(void)
 {
	if (IS_ENABLED(CONFIG_RADIO_LOADER_MODE_MPC)) {
		/* Configure the MPC to load the radio firmware into TCM from the app core. */
		printk("Configuring the MPC to load the radio firmware into TCM from the app core.\n");
		radio_tcm_mpc_configure();

		/* Set the magic to the MPC ready magic. */
		printk("Setting the magic to the MPC ready magic.\n");
		shared_magic_set_mpc_ready();

		/* Wait for the magic to boot up. */
		printk("Waiting for the loader to copy the firmware to the TCM.\n");
		shared_magic_wait_for_boot_ready();
		printk("Loader copied the firmware to the TCM.\n");

		/* Consume the magic. */
		shared_magic_consume();

	} else if (IS_ENABLED(CONFIG_RADIO_LOADER_MODE_COPY)) {
		printk("Copying the firmware to the RAM.\n");
		memcpy((void *)LOADED_FW_RAM_ADDR, (void *)LOADED_FW_ADDR, LOADED_FW_SIZE);
	}

	printk("Jumping to the firmware.\n");
	k_msleep(2000);

	uint32_t *vector_table = (uint32_t *)LOADED_FW_RAM_ADDR;
	void (*reset_handler)(void) = (void (*)(void))(vector_table[1]);
	reset_handler();

	/* should never reach here */
	return 0;
 }


int main(void)
{
	wait_loop();

	// Should never reach here
	printk("ERROR: Firmware jump failed!\n");
	return -1;
}

//  SYS_INIT(wait_loop, EARLY, 0);
