/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

 #include <zephyr/kernel.h>

 
#define LOADED_FW_NODE        		DT_CHOSEN(zephyr_loaded_fw_src)
#define LOADED_FW_PARTITION_NODE 	DT_PARENT(DT_PARENT(LOADED_FW_NODE))
#define LOADED_FW_ADDR        		(DT_REG_ADDR(LOADED_FW_NODE) + DT_REG_ADDR(LOADED_FW_PARTITION_NODE))
#define LOADED_FW_SIZE        		DT_REG_SIZE(LOADED_FW_NODE)

#define LOADED_FW_RAM_NODE    		DT_CHOSEN(zephyr_loaded_fw_dst)
#define LOADED_FW_RAM_ADDR    		DT_REG_ADDR(LOADED_FW_RAM_NODE)
#define LOADED_FW_RAM_SIZE    		DT_REG_SIZE(LOADED_FW_RAM_NODE)
 
  
int main(void)
{
	// Should never reach here
	printk("ERROR: Firmware jump failed!\n");
	return -1;
}
 
 static int wait_loop(void)
 {
	memcpy((void *)LOADED_FW_RAM_ADDR, (void *)LOADED_FW_ADDR, LOADED_FW_SIZE);

	uint32_t *vector_table = (uint32_t *)LOADED_FW_RAM_ADDR;
	void (*reset_handler)(void) = (void (*)(void))(vector_table[1]);
	reset_handler();
 
	/* should never reach here */
	return 0;
 }
 
 
 SYS_INIT(wait_loop, EARLY, 0);