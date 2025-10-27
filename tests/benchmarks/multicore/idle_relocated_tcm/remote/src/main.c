/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(idle);

int main(void)
{
	unsigned int cnt = 0;


	LOG_INF("Multicore idle test on %s", CONFIG_BOARD_TARGET);
	uintptr_t pc;
	__asm__ volatile("adr %0, ." : "=r"(pc));
	printk("Current PC (program counter) address: 0x%lx\n", (unsigned long)pc);
	while (1) {
		LOG_INF("Multicore idle test iteration %u", cnt++);
		k_msleep(2000);
	}

	return 0;
}
