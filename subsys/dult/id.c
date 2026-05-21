/*
 * Copyright (c) 2024-2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <errno.h>

#include <zephyr/kernel.h>

#include <dult/dult.h>
#include "dult_user.h"
#include "dult_id.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dult_id, CONFIG_DULT_LOG_LEVEL);

struct id_state {
	const struct dult_id_read_state_cb *cb;
	struct k_work_delayable timeout_work;
	bool is_enabled;
	bool in_read_state;
	size_t slot_idx;
};

static struct id_state states[CONFIG_DULT_USER_MAX];

static void id_read_state_timeout_handle(struct k_work *work);

static void state_init(void)
{
	static bool initialized;

	if (initialized) {
		return;
	}
	initialized = true;

	for (size_t i = 0; i < ARRAY_SIZE(states); i++) {
		states[i].slot_idx = i;
		k_work_init_delayable(&states[i].timeout_work, id_read_state_timeout_handle);
	}
}

static void id_read_state_timeout_handle(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct id_state *st = CONTAINER_OF(dwork, struct id_state, timeout_work);
	const struct dult_user *user;

	user = dult_user_by_slot_idx(st->slot_idx);
	__ASSERT_NO_MSG(user);
	__ASSERT_NO_MSG(dult_user_is_ready(user));
	__ASSERT_NO_MSG(st->in_read_state);

	st->in_read_state = false;
	if (st->cb && st->cb->exited) {
		st->cb->exited();
	}
}

int dult_id_read_state_cb_register(const struct dult_user *user,
				   const struct dult_id_read_state_cb *cb)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	state_init();

	if (dult_user_is_ready(user)) {
		LOG_ERR("DULT ID: module must be disabled to register callbacks");
		return -EACCES;
	}

	if (states[idx].cb) {
		LOG_ERR("DULT ID: identifier read state callback already registered");
		return -EALREADY;
	}

	if (!cb || !cb->payload_get || !cb->exited) {
		return -EINVAL;
	}

	states[idx].cb = cb;

	return 0;
}

int dult_id_read_state_enter(const struct dult_user *user)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	if (!dult_user_is_ready(user)) {
		LOG_ERR("DULT ID: module is not enabled");
		return -EACCES;
	}

	states[idx].in_read_state = true;
	(void) k_work_reschedule(&states[idx].timeout_work,
				 K_MINUTES(CONFIG_DULT_ID_READ_STATE_TIMEOUT));

	return 0;
}

bool dult_id_is_in_read_state(const struct dult_user *user)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return false;
	}

	__ASSERT_NO_MSG(dult_user_is_ready(user));

	return states[idx].in_read_state;
}

int dult_id_payload_get(const struct dult_user *user, uint8_t *buf, size_t *len)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	__ASSERT_NO_MSG(dult_user_is_ready(user));
	__ASSERT_NO_MSG(states[idx].in_read_state);

	if (!states[idx].cb || !states[idx].cb->payload_get) {
		return -EACCES;
	}

	return states[idx].cb->payload_get(buf, len);
}

int dult_id_enable(const struct dult_user *user)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	state_init();

	if (states[idx].is_enabled) {
		LOG_ERR("DULT ID: already enabled");
		return -EALREADY;
	}

	if (!states[idx].cb) {
		LOG_ERR("DULT ID: callbacks must be registered at this point");
		return -EINVAL;
	}

	states[idx].is_enabled = true;

	return 0;
}

int dult_id_reset(const struct dult_user *user)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	if (!states[idx].is_enabled) {
		LOG_ERR("DULT ID: is not enabled");
		return -EALREADY;
	}

	states[idx].is_enabled = false;
	if (states[idx].in_read_state) {
		states[idx].in_read_state = false;
		if (states[idx].cb && states[idx].cb->exited) {
			states[idx].cb->exited();
		}
	}
	(void) k_work_cancel_delayable(&states[idx].timeout_work);

	states[idx].cb = NULL;

	return 0;
}
