/*
 * Copyright (c) 2024-2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <zephyr/kernel.h>
#include <errno.h>

#include "dult_bt_anos.h"
#include "dult_motion_detector.h"
#include "dult_user.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dult_sound, CONFIG_DULT_LOG_LEVEL);

struct sound_state {
	const struct dult_sound_cb *sound_cb;
	enum dult_sound_src sound_src;
	bool sound_active;
	bool is_enabled;
};

static struct sound_state states[CONFIG_DULT_USER_MAX];

int dult_sound_cb_register(const struct dult_user *user, const struct dult_sound_cb *cb)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	if (dult_user_is_ready(user)) {
		LOG_ERR("DULT Sound: module must be disabled to register callbacks");
		return -EACCES;
	}
	__ASSERT_NO_MSG(!states[idx].is_enabled);

	if (states[idx].sound_cb) {
		LOG_ERR("DULT Sound: sound callbacks already registered");
		return -EALREADY;
	}

	if (!cb || !cb->sound_start || !cb->sound_stop) {
		return -EINVAL;
	}

	states[idx].sound_cb = cb;

	return 0;
}

int dult_sound_state_update(const struct dult_user *user,
			    const struct dult_sound_state_param *param)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	if (!dult_user_is_ready(user)) {
		LOG_ERR("DULT Sound: module is not enabled");
		return -EACCES;
	}
	__ASSERT_NO_MSG(states[idx].is_enabled);

	if (param->active == states[idx].sound_active) {
		if (param->src == states[idx].sound_src) {
			LOG_WRN("DULT Sound: state has not changed");
			return 0;
		}

		if (!states[idx].sound_active) {
			LOG_WRN("DULT Sound: unnecessary source change when sound is not active");
			return 0;
		}
	}

	states[idx].sound_active = param->active;
	states[idx].sound_src = param->src;

	dult_bt_anos_sound_state_change_notify(user, param->active,
					       param->src == DULT_SOUND_SRC_BT_GATT);

	if (IS_ENABLED(CONFIG_DULT_MOTION_DETECTOR)) {
		dult_motion_detector_sound_state_change_notify(
				user,
				param->active,
				param->src == DULT_SOUND_SRC_MOTION_DETECTOR);
	}

	return 0;
}

static void anos_sound_start(const struct dult_user *user)
{
	size_t idx = dult_user_slot_idx(user);

	__ASSERT_NO_MSG(idx != DULT_USER_SLOT_NONE);
	if (idx == DULT_USER_SLOT_NONE) {
		return;
	}

	if (states[idx].sound_active) {
		dult_bt_anos_sound_state_change_notify(user, states[idx].sound_active, false);
		return;
	}

	__ASSERT(states[idx].sound_cb && states[idx].sound_cb->sound_start,
		 "DULT Sound: start callback is not populated");

	states[idx].sound_cb->sound_start(DULT_SOUND_SRC_BT_GATT);
}

static void anos_sound_stop(const struct dult_user *user)
{
	size_t idx = dult_user_slot_idx(user);

	__ASSERT_NO_MSG(idx != DULT_USER_SLOT_NONE);
	if (idx == DULT_USER_SLOT_NONE) {
		return;
	}

	__ASSERT(states[idx].sound_cb && states[idx].sound_cb->sound_stop,
		 "DULT Sound: stop callback is not populated");

	states[idx].sound_cb->sound_stop(DULT_SOUND_SRC_BT_GATT);
}

static const struct dult_bt_anos_sound_cb anos_sound_cb = {
	.sound_start = anos_sound_start,
	.sound_stop = anos_sound_stop,
};

static void motion_detector_sound_start(const struct dult_user *user)
{
	size_t idx;

	__ASSERT_NO_MSG(IS_ENABLED(CONFIG_DULT_MOTION_DETECTOR));

	idx = dult_user_slot_idx(user);
	__ASSERT_NO_MSG(idx != DULT_USER_SLOT_NONE);
	if (idx == DULT_USER_SLOT_NONE) {
		return;
	}

	if (states[idx].sound_active) {
		dult_motion_detector_sound_state_change_notify(user,
							       states[idx].sound_active,
							       false);
		return;
	}

	__ASSERT(states[idx].sound_cb && states[idx].sound_cb->sound_start,
		 "DULT Sound: start callback is not populated");

	states[idx].sound_cb->sound_start(DULT_SOUND_SRC_MOTION_DETECTOR);
}

static const struct dult_motion_detector_sound_cb motion_detector_sound_cb = {
	.sound_start = motion_detector_sound_start,
};

int dult_sound_enable(const struct dult_user *user)
{
	static bool bt_anos_sound_cb_registered;
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	if (!bt_anos_sound_cb_registered) {
		dult_bt_anos_sound_cb_register(&anos_sound_cb);
		bt_anos_sound_cb_registered = true;
	}

	if (IS_ENABLED(CONFIG_DULT_MOTION_DETECTOR)) {
		static bool motion_detector_sound_cb_registered;

		if (!motion_detector_sound_cb_registered) {
			dult_motion_detector_sound_cb_register(&motion_detector_sound_cb);
			motion_detector_sound_cb_registered = true;
		}
	}

	if (states[idx].is_enabled) {
		LOG_ERR("DULT Sound: already enabled");
		return -EALREADY;
	}

	if (!states[idx].sound_cb) {
		LOG_ERR("DULT Sound: callbacks must be registered at this point");
		return -EINVAL;
	}

	states[idx].is_enabled = true;

	return 0;
}

int dult_sound_reset(const struct dult_user *user)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	if (!states[idx].is_enabled) {
		LOG_ERR("DULT Sound: already disabled");
		return -EALREADY;
	}

	states[idx].is_enabled = false;
	states[idx].sound_cb = NULL;
	states[idx].sound_active = false;
	states[idx].sound_src = DULT_SOUND_SRC_BT_GATT;

	return 0;
}
