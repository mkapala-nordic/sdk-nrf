/*
 * Copyright (c) 2024-2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <zephyr/kernel.h>
#include <errno.h>

#include "dult_user.h"
#include "dult_battery.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dult_battery, CONFIG_DULT_LOG_LEVEL);

/** Special values for the battery level. */
#define DULT_BATTERY_LEVEL_NONE (0xFF)
#define DULT_BATTERY_LEVEL_MAX  (100U)

/** Special values for the battery type. */
#define DULT_BATTERY_TYPE_INVALID (0xFF)

/* Battery Type encoding */
enum dult_battery_type {
	DULT_BATTERY_TYPE_POWERED = 0x00,
	DULT_BATTERY_TYPE_NON_RECHARGEABLE = 0x01,
	DULT_BATTERY_TYPE_RECHARGEABLE = 0x02,
};

/* Battery Level encoding */
enum dult_battery_level {
	DULT_BATTERY_LEVEL_FULL = 0x00,
	DULT_BATTERY_LEVEL_MEDIUM = 0x01,
	DULT_BATTERY_LEVEL_LOW = 0x02,
	DULT_BATTERY_LEVEL_CRITICAL = 0x03,
};

BUILD_ASSERT(CONFIG_DULT_BATTERY_LEVEL_MEDIUM_THR >=
	     CONFIG_DULT_BATTERY_LEVEL_LOW_THR);
BUILD_ASSERT(CONFIG_DULT_BATTERY_LEVEL_LOW_THR >=
	     CONFIG_DULT_BATTERY_LEVEL_CRITICAL_THR);

struct battery_state {
	uint8_t level;
	bool is_enabled;
};

static struct battery_state state[CONFIG_DULT_USER_MAX] = {
	[0 ... CONFIG_DULT_USER_MAX - 1] = { .level = DULT_BATTERY_LEVEL_NONE },
};

uint8_t dult_battery_type_encode(void)
{
	if (IS_ENABLED(CONFIG_DULT_BATTERY_TYPE_POWERED)) {
		return DULT_BATTERY_TYPE_POWERED;
	} else if (IS_ENABLED(CONFIG_DULT_BATTERY_TYPE_NON_RECHARGEABLE)) {
		return DULT_BATTERY_TYPE_NON_RECHARGEABLE;
	} else if (IS_ENABLED(CONFIG_DULT_BATTERY_TYPE_RECHARGEABLE)) {
		return DULT_BATTERY_TYPE_RECHARGEABLE;
	}

	__ASSERT(0, "DULT Battery: battery type invalid configuration");
	return DULT_BATTERY_TYPE_INVALID;
}

uint8_t dult_battery_level_encode(const struct dult_user *user)
{
	size_t idx = dult_user_slot_idx(user);
	uint8_t level;

	if (idx == DULT_USER_SLOT_NONE) {
		return DULT_BATTERY_LEVEL_CRITICAL;
	}

	level = state[idx].level;
	__ASSERT(level <= DULT_BATTERY_LEVEL_MAX,
		 "DULT Battery: incorrect battery level in %%");

	if (level <= CONFIG_DULT_BATTERY_LEVEL_CRITICAL_THR) {
		return DULT_BATTERY_LEVEL_CRITICAL;
	} else if (level <= CONFIG_DULT_BATTERY_LEVEL_LOW_THR) {
		return DULT_BATTERY_LEVEL_LOW;
	} else if (level <= CONFIG_DULT_BATTERY_LEVEL_MEDIUM_THR) {
		return DULT_BATTERY_LEVEL_MEDIUM;
	} else {
		return DULT_BATTERY_LEVEL_FULL;
	}
}

int dult_battery_level_set(const struct dult_user *user, uint8_t percentage_level)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	if (percentage_level > DULT_BATTERY_LEVEL_MAX) {
		LOG_ERR("DULT Battery: incorrect battery level in %d %%", percentage_level);
		return -EINVAL;
	}

	state[idx].level = percentage_level;

	return 0;
}

int dult_battery_enable(const struct dult_user *user)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	if (state[idx].is_enabled) {
		LOG_ERR("DULT Battery: already enabled");
		return -EALREADY;
	}

	if (state[idx].level == DULT_BATTERY_LEVEL_NONE) {
		LOG_ERR("DULT Battery: battery level unset before the enable operation");
		return -EINVAL;
	}

	state[idx].is_enabled = true;

	return 0;
}

int dult_battery_reset(const struct dult_user *user)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	if (!state[idx].is_enabled) {
		LOG_ERR("DULT Battery: already disabled");
		return -EALREADY;
	}

	state[idx].is_enabled = false;
	state[idx].level = DULT_BATTERY_LEVEL_NONE;

	return 0;
}
