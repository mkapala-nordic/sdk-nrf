/*
 * Copyright (c) 2024-2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/slist.h>

#include <dult/dult.h>
#include "dult_near_owner_state.h"
#include "dult_user.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dult_near_owner_state, CONFIG_DULT_LOG_LEVEL);

static enum dult_near_owner_state_mode cur_modes[CONFIG_DULT_USER_MAX] = {
	[0 ... CONFIG_DULT_USER_MAX - 1] = DULT_NEAR_OWNER_STATE_MODE_NEAR_OWNER,
};
static sys_slist_t state_cb_slist = SYS_SLIST_STATIC_INIT(&state_cb_slist);

static bool node_uniqueness_validate(sys_slist_t *slist, sys_snode_t *new_node)
{
	sys_snode_t *current_node;

	SYS_SLIST_FOR_EACH_NODE(slist, current_node) {
		if (current_node == new_node) {
			return false;
		}
	}

	return true;
}

void dult_near_owner_state_cb_register(struct dult_near_owner_state_cb *cb)
{
	__ASSERT_NO_MSG(cb && cb->state_changed);

	if (!node_uniqueness_validate(&state_cb_slist, &cb->node)) {
		__ASSERT_NO_MSG(false);
		return;
	}

	sys_slist_append(&state_cb_slist, &cb->node);
}

int dult_near_owner_state_set(const struct dult_user *user, enum dult_near_owner_state_mode mode)
{
	size_t idx;
	enum dult_near_owner_state_mode prev_mode;

	idx = dult_user_slot_idx(user);
	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	prev_mode = cur_modes[idx];
	cur_modes[idx] = mode;
	if (prev_mode != mode) {
		struct dult_near_owner_state_cb *listener;

		SYS_SLIST_FOR_EACH_CONTAINER(&state_cb_slist, listener, node) {
			listener->state_changed(user, mode);
		}
	}

	return 0;
}

enum dult_near_owner_state_mode dult_near_owner_state_get(const struct dult_user *user)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return DULT_NEAR_OWNER_STATE_MODE_NEAR_OWNER;
	}

	return cur_modes[idx];
}

void dult_near_owner_state_reset(const struct dult_user *user)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return;
	}

	cur_modes[idx] = DULT_NEAR_OWNER_STATE_MODE_NEAR_OWNER;
}
