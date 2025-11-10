#ifndef SHARED_MAGIC_H
#define SHARED_MAGIC_H

#include <zephyr/kernel.h>
#include <zephyr/cache.h>

// #define RADIO_LOADER_MAGIC_NODE 	DT_CHOSEN(zephyr_sharable_magic_sram)
#define RADIO_LOADER_MAGIC_ADDR 	0x2f039000
#define RADIO_LOADER_MAGIC_MPC_READY	0x55555555
#define RADIO_LOADER_MAGIC_BOOT_READY	0xdeadbeef

static volatile uint32_t *shared_magic_addr = (uint32_t *)RADIO_LOADER_MAGIC_ADDR;

static inline uint32_t shared_magic_get(void)
{
	sys_cache_data_invd_range((void *)shared_magic_addr, sizeof(*shared_magic_addr));
	return *shared_magic_addr;
}

static inline void shared_magic_set(uint32_t value)
{
	*shared_magic_addr = value;
	sys_cache_data_flush_range((void *)shared_magic_addr, sizeof(*shared_magic_addr));
}

static inline void shared_magic_set_mpc_ready(void)
{
	shared_magic_set(RADIO_LOADER_MAGIC_MPC_READY);
}

static inline void shared_magic_set_boot_ready(void)
{
	shared_magic_set(RADIO_LOADER_MAGIC_BOOT_READY);
}

static inline void shared_magic_consume(void)
{
	shared_magic_set(0);
}

static inline void shared_magic_wait_for(uint32_t value)
{
	while (shared_magic_get() != value) {
		k_msleep(100);
	}
}

static inline void shared_magic_wait_for_boot_ready(void)
{
	shared_magic_wait_for(RADIO_LOADER_MAGIC_BOOT_READY);
}

static inline void shared_magic_wait_for_mpc_ready(void)
{
	shared_magic_wait_for(RADIO_LOADER_MAGIC_MPC_READY);
}

#endif // SHARED_MAGIC_H
