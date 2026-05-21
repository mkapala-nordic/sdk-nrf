/*
 * Copyright (c) 2024-2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <errno.h>

#include <dult/dult.h>
#include <dult/bt.h>
#include "dult_battery.h"
#include "dult_bt_anos.h"
#include "dult_id.h"
#include "dult_motion_detector.h"
#include "dult_near_owner_state.h"
#include "dult_sound.h"
#include "dult_user.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dult_user, CONFIG_DULT_LOG_LEVEL);

BUILD_ASSERT(CONFIG_DULT_USER_MAX >= 1, "CONFIG_DULT_USER_MAX must be at least 1");

struct dult_user_slot {
	const struct dult_user *user;
	const struct dult_anos_cb *anos_cb;
	bool is_enabled;
};

static struct dult_user_slot slots[CONFIG_DULT_USER_MAX];
static K_MUTEX_DEFINE(slots_lock);

static size_t slot_idx_locked(const struct dult_user *user)
{
	if (!user) {
		return DULT_USER_SLOT_NONE;
	}

	for (size_t i = 0; i < ARRAY_SIZE(slots); i++) {
		if (slots[i].user == user) {
			return i;
		}
	}

	return DULT_USER_SLOT_NONE;
}

size_t dult_user_slot_idx(const struct dult_user *user)
{
	size_t idx;

	k_mutex_lock(&slots_lock, K_FOREVER);
	idx = slot_idx_locked(user);
	k_mutex_unlock(&slots_lock);

	return idx;
}

const struct dult_user *dult_user_by_slot_idx(size_t slot_idx)
{
	const struct dult_user *user;

	if (slot_idx >= ARRAY_SIZE(slots)) {
		return NULL;
	}

	k_mutex_lock(&slots_lock, K_FOREVER);
	user = slots[slot_idx].user;
	k_mutex_unlock(&slots_lock);

	return user;
}

bool dult_user_is_registered(const struct dult_user *user)
{
	return (dult_user_slot_idx(user) != DULT_USER_SLOT_NONE);
}

const struct dult_user *dult_user_get(void)
{
	const struct dult_user *user = NULL;
	size_t count = 0;

	k_mutex_lock(&slots_lock, K_FOREVER);
	for (size_t i = 0; i < ARRAY_SIZE(slots); i++) {
		if (slots[i].user) {
			user = slots[i].user;
			count++;
		}
	}
	k_mutex_unlock(&slots_lock);

	__ASSERT(count <= 1,
		 "dult_user_get() is ambiguous when more than one user is registered");

	return (count == 1) ? user : NULL;
}

bool dult_user_is_ready(const struct dult_user *user)
{
	bool ready = false;

	k_mutex_lock(&slots_lock, K_FOREVER);
	if (user) {
		size_t idx = slot_idx_locked(user);

		if (idx != DULT_USER_SLOT_NONE) {
			ready = slots[idx].is_enabled;
		}
	} else {
		/* Backwards-compatible "any user ready" form. */
		for (size_t i = 0; i < ARRAY_SIZE(slots); i++) {
			if (slots[i].user && slots[i].is_enabled) {
				ready = true;
				break;
			}
		}
	}
	k_mutex_unlock(&slots_lock);

	return ready;
}

void dult_user_for_each(dult_user_iter_cb_t cb, void *user_data)
{
	const struct dult_user *snapshot[CONFIG_DULT_USER_MAX];
	size_t count = 0;

	if (!cb) {
		return;
	}

	k_mutex_lock(&slots_lock, K_FOREVER);
	for (size_t i = 0; i < ARRAY_SIZE(slots); i++) {
		if (slots[i].user) {
			snapshot[count++] = slots[i].user;
		}
	}
	k_mutex_unlock(&slots_lock);

	for (size_t i = 0; i < count; i++) {
		cb(snapshot[i], user_data);
	}
}

const struct dult_user *dult_user_lookup_by_conn(struct bt_conn *conn)
{
	const struct dult_user *match = NULL;
	const struct dult_user *fallback = NULL;
	size_t registered = 0;

	if (!conn) {
		return NULL;
	}

	k_mutex_lock(&slots_lock, K_FOREVER);
	for (size_t i = 0; i < ARRAY_SIZE(slots); i++) {
		const struct dult_user *user = slots[i].user;
		const struct dult_anos_cb *cb = slots[i].anos_cb;

		if (!user) {
			continue;
		}

		registered++;
		fallback = user;

		if (cb && cb->owns_connection && cb->owns_connection(conn)) {
			match = user;
			break;
		}
	}
	k_mutex_unlock(&slots_lock);

	if (match) {
		return match;
	}

	/* Single-user compatibility fallback: when only one user is registered and it
	 * either does not implement owns_connection or does not claim this connection,
	 * route the operation to that user.
	 */
	if (registered == 1) {
		return fallback;
	}

	return NULL;
}

int dult_user_register(const struct dult_user *user)
{
	int err = 0;

	if (!user) {
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_ASSERT)) {
		size_t name_len;

		name_len = strnlen(user->manufacturer_name, DULT_USER_STR_PARAM_LEN_MAX + 1);
		__ASSERT_NO_MSG((name_len > 0) && (name_len <= DULT_USER_STR_PARAM_LEN_MAX));

		name_len = strnlen(user->model_name, DULT_USER_STR_PARAM_LEN_MAX + 1);
		__ASSERT_NO_MSG((name_len > 0) && (name_len <= DULT_USER_STR_PARAM_LEN_MAX));
	}

	if (!(user->accessory_capabilities & BIT(DULT_ACCESSORY_CAPABILITY_PLAY_SOUND_BIT_POS))) {
		return -EINVAL;
	}

	if (!(user->accessory_capabilities &
	      BIT(DULT_ACCESSORY_CAPABILITY_ID_LOOKUP_NFC_BIT_POS)) &&
	    !(user->accessory_capabilities &
	      BIT(DULT_ACCESSORY_CAPABILITY_ID_LOOKUP_BLE_BIT_POS))) {
		return -EINVAL;
	}

	k_mutex_lock(&slots_lock, K_FOREVER);

	if (slot_idx_locked(user) != DULT_USER_SLOT_NONE) {
		LOG_ERR("DULT user already registered");
		err = -EALREADY;
		goto out;
	}

	for (size_t i = 0; i < ARRAY_SIZE(slots); i++) {
		if (!slots[i].user) {
			slots[i].user = user;
			slots[i].anos_cb = NULL;
			slots[i].is_enabled = false;
			goto out;
		}
	}

	LOG_ERR("DULT user registry full (CONFIG_DULT_USER_MAX=%d)", CONFIG_DULT_USER_MAX);
	err = -ENOMEM;

out:
	k_mutex_unlock(&slots_lock);
	return err;
}

int dult_anos_cb_register(const struct dult_user *user, const struct dult_anos_cb *cb)
{
	int err = 0;

	if (!user || !cb) {
		return -EINVAL;
	}

	if (CONFIG_DULT_USER_MAX > 1 && !cb->owns_connection) {
		LOG_ERR("DULT ANOS: owns_connection callback is mandatory in multi-user builds");
		return -EINVAL;
	}

	k_mutex_lock(&slots_lock, K_FOREVER);

	size_t idx = slot_idx_locked(user);

	if (idx == DULT_USER_SLOT_NONE) {
		err = -EACCES;
		goto out;
	}

	if (slots[idx].is_enabled) {
		LOG_ERR("DULT ANOS: cannot register callbacks for an already enabled user");
		err = -EACCES;
		goto out;
	}

	if (slots[idx].anos_cb) {
		LOG_ERR("DULT ANOS: callback already registered for user");
		err = -EALREADY;
		goto out;
	}

	slots[idx].anos_cb = cb;

out:
	k_mutex_unlock(&slots_lock);
	return err;
}

const struct dult_anos_cb *dult_user_anos_cb_get(const struct dult_user *user)
{
	const struct dult_anos_cb *cb = NULL;
	size_t idx;

	k_mutex_lock(&slots_lock, K_FOREVER);
	idx = slot_idx_locked(user);
	if (idx != DULT_USER_SLOT_NONE) {
		cb = slots[idx].anos_cb;
	}
	k_mutex_unlock(&slots_lock);

	return cb;
}

int dult_enable(const struct dult_user *user)
{
	int err;
	size_t idx;

	__ASSERT_NO_MSG(!k_is_preempt_thread());
	__ASSERT_NO_MSG(!k_is_in_isr());

	if (!user) {
		return -EINVAL;
	}

	k_mutex_lock(&slots_lock, K_FOREVER);
	idx = slot_idx_locked(user);
	if (idx == DULT_USER_SLOT_NONE) {
		k_mutex_unlock(&slots_lock);
		return -EACCES;
	}
	if (slots[idx].is_enabled) {
		LOG_ERR("DULT already enabled");
		k_mutex_unlock(&slots_lock);
		return -EALREADY;
	}
	k_mutex_unlock(&slots_lock);

	if (IS_ENABLED(CONFIG_DULT_BATTERY)) {
		err = dult_battery_enable(user);
		if (err) {
			LOG_ERR("dult_battery_enable returned an error: %d", err);
			return err;
		}
	}

	if (IS_ENABLED(CONFIG_DULT_MOTION_DETECTOR) &&
	    (user->accessory_capabilities &
	     BIT(DULT_ACCESSORY_CAPABILITY_MOTION_DETECTOR_UT_BIT_POS))) {
		err = dult_motion_detector_enable(user);
		if (err) {
			LOG_ERR("dult_motion_detector_enable returned an error: %d", err);
			return err;
		}
	}

	err = dult_bt_anos_enable(user);
	if (err) {
		LOG_ERR("dult_bt_anos_enable returned an error: %d", err);
		return err;
	}

	err = dult_sound_enable(user);
	if (err) {
		LOG_ERR("dult_sound_enable returned an error: %d", err);
		return err;
	}

	err = dult_id_enable(user);
	if (err) {
		LOG_ERR("dult_id_enable returned an error: %d", err);
		return err;
	}

	k_mutex_lock(&slots_lock, K_FOREVER);
	slots[idx].is_enabled = true;
	k_mutex_unlock(&slots_lock);

	LOG_INF("DULT enabled (slot %u)", (unsigned int)idx);

	return 0;
}

int dult_reset(const struct dult_user *user)
{
	int err;
	size_t idx;
	bool was_enabled;

	__ASSERT_NO_MSG(!k_is_preempt_thread());
	__ASSERT_NO_MSG(!k_is_in_isr());

	if (!user) {
		return -EINVAL;
	}

	k_mutex_lock(&slots_lock, K_FOREVER);
	idx = slot_idx_locked(user);
	if (idx == DULT_USER_SLOT_NONE) {
		k_mutex_unlock(&slots_lock);
		return -EACCES;
	}
	was_enabled = slots[idx].is_enabled;
	k_mutex_unlock(&slots_lock);

	if (was_enabled) {
		dult_near_owner_state_reset(user);

		err = dult_id_reset(user);
		if (err) {
			LOG_ERR("dult_id_reset returned an error: %d", err);
			return err;
		}

		err = dult_sound_reset(user);
		if (err) {
			LOG_ERR("dult_sound_reset returned an error: %d", err);
			return err;
		}

		err = dult_bt_anos_reset(user);
		if (err) {
			LOG_ERR("dult_bt_anos_reset returned an error: %d", err);
			return err;
		}

		if (IS_ENABLED(CONFIG_DULT_MOTION_DETECTOR) &&
		    (user->accessory_capabilities &
		     BIT(DULT_ACCESSORY_CAPABILITY_MOTION_DETECTOR_UT_BIT_POS))) {
			err = dult_motion_detector_reset(user);
			if (err) {
				LOG_ERR("dult_motion_detector_reset returned an error: %d", err);
				return err;
			}
		}

		if (IS_ENABLED(CONFIG_DULT_BATTERY)) {
			err = dult_battery_reset(user);
			if (err) {
				LOG_ERR("dult_battery_reset returned an error: %d", err);
				return err;
			}
		}
	}

	k_mutex_lock(&slots_lock, K_FOREVER);
	slots[idx].is_enabled = false;
	slots[idx].anos_cb = NULL;
	slots[idx].user = NULL;
	k_mutex_unlock(&slots_lock);

	LOG_INF("DULT reset completed (slot %u)", (unsigned int)idx);

	return 0;
}
