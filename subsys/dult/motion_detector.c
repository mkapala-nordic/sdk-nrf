/*
 * Copyright (c) 2024-2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/random/random.h>

#include <dult/dult.h>
#include "dult_user.h"
#include "dult_motion_detector.h"
#include "dult_near_owner_state.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dult_motion_detector, CONFIG_DULT_LOG_LEVEL);

/* Sampling rate in MOTION_POLL_STATE_PASSIVE state. */
#define SEPARATED_UT_SAMPLING_RATE1	\
	K_MSEC(CONFIG_DULT_MOTION_DETECTOR_SEPARATED_UT_SAMPLING_RATE1)
/* Sampling rate in MOTION_POLL_STATE_ACTIVE state. */
#define SEPARATED_UT_SAMPLING_RATE2	\
	K_MSEC(CONFIG_DULT_MOTION_DETECTOR_SEPARATED_UT_SAMPLING_RATE2)
#define SEPARATED_UT_BACKOFF_PERIOD	\
	K_MINUTES(CONFIG_DULT_MOTION_DETECTOR_SEPARATED_UT_BACKOFF_PERIOD)

#define SEPARATED_UT_TIMEOUT_PERIOD_MIN		\
	(CONFIG_DULT_MOTION_DETECTOR_SEPARATED_UT_TIMEOUT_PERIOD_MIN)
#define SEPARATED_UT_TIMEOUT_PERIOD_MAX		\
	(CONFIG_DULT_MOTION_DETECTOR_SEPARATED_UT_TIMEOUT_PERIOD_MAX)
#define SEPARATED_UT_TIMEOUT_PERIOD_DIFF	\
	(SEPARATED_UT_TIMEOUT_PERIOD_MAX - SEPARATED_UT_TIMEOUT_PERIOD_MIN)

#define SEPARATED_UT_ACTIVE_POLL_DURATION	\
	K_SECONDS(CONFIG_DULT_MOTION_DETECTOR_SEPARATED_UT_ACTIVE_POLL_DURATION)
#define SEPARATED_UT_MAX_SOUND_COUNT		\
	CONFIG_DULT_MOTION_DETECTOR_SEPARATED_UT_MAX_SOUND_COUNT

BUILD_ASSERT(SEPARATED_UT_TIMEOUT_PERIOD_DIFF >= 0);

#if CONFIG_DULT_MOTION_DETECTOR_TEST_MODE
static uint32_t ut_backoff_period_min =
	CONFIG_DULT_MOTION_DETECTOR_SEPARATED_UT_BACKOFF_PERIOD;
static uint32_t ut_timeout_period_min =
	CONFIG_DULT_MOTION_DETECTOR_SEPARATED_UT_TIMEOUT_PERIOD_MIN;
static uint32_t ut_timeout_period_max =
	CONFIG_DULT_MOTION_DETECTOR_SEPARATED_UT_TIMEOUT_PERIOD_MAX;
#endif /* CONFIG_DULT_MOTION_DETECTOR_TEST_MODE */

static k_timeout_t backoff_period_get(void)
{
#if CONFIG_DULT_MOTION_DETECTOR_TEST_MODE
	return K_MINUTES(ut_backoff_period_min);
#else
	return SEPARATED_UT_BACKOFF_PERIOD;
#endif
}

static uint32_t timeout_period_min_get(void)
{
#if CONFIG_DULT_MOTION_DETECTOR_TEST_MODE
	return ut_timeout_period_min;
#else
	return SEPARATED_UT_TIMEOUT_PERIOD_MIN;
#endif
}

static uint32_t timeout_period_max_get(void)
{
#if CONFIG_DULT_MOTION_DETECTOR_TEST_MODE
	return ut_timeout_period_max;
#else
	return SEPARATED_UT_TIMEOUT_PERIOD_MAX;
#endif
}

enum motion_poll_state {
	MOTION_POLL_STATE_STOPPED,
	MOTION_POLL_STATE_PASSIVE,
	MOTION_POLL_STATE_PASSIVE_SOUND_REQUESTED,
	MOTION_POLL_STATE_ACTIVE,
	MOTION_POLL_STATE_ACTIVE_SOUND_REQUESTED,
};

struct md_state {
	const struct dult_motion_detector_cb *motion_detector_cb;
	struct k_work_delayable motion_enable_work;
	struct k_work_delayable motion_poll_work;
	struct k_work_delayable motion_poll_duration_work;
	enum motion_poll_state poll_state;
	uint8_t sound_count;
	bool is_enabled;
	size_t slot_idx;
};

static struct md_state states[CONFIG_DULT_USER_MAX];
static const struct dult_motion_detector_sound_cb *sound_cb;

static void motion_enable_work_handle(struct k_work *work);
static void motion_poll_work_handle(struct k_work *work);
static void motion_poll_duration_work_handle(struct k_work *work);

static void state_init(void)
{
	static bool initialized;

	if (initialized) {
		return;
	}
	initialized = true;

	for (size_t i = 0; i < ARRAY_SIZE(states); i++) {
		states[i].slot_idx = i;
		states[i].poll_state = MOTION_POLL_STATE_STOPPED;
		k_work_init_delayable(&states[i].motion_enable_work,
				      motion_enable_work_handle);
		k_work_init_delayable(&states[i].motion_poll_work,
				      motion_poll_work_handle);
		k_work_init_delayable(&states[i].motion_poll_duration_work,
				      motion_poll_duration_work_handle);
	}
}

static struct md_state *state_from_enable_work(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);

	return CONTAINER_OF(dwork, struct md_state, motion_enable_work);
}

static struct md_state *state_from_poll_work(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);

	return CONTAINER_OF(dwork, struct md_state, motion_poll_work);
}

static struct md_state *state_from_poll_duration_work(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);

	return CONTAINER_OF(dwork, struct md_state, motion_poll_duration_work);
}

static const struct dult_user *state_user(struct md_state *st)
{
	return dult_user_by_slot_idx(st->slot_idx);
}

static void motion_enable_work_handle(struct k_work *work)
{
	struct md_state *st = state_from_enable_work(work);
	int ret;

	LOG_DBG("Enabling the motion detector");

	__ASSERT(st->motion_detector_cb, "Motion detector callback structure is not registered");
	__ASSERT(st->motion_detector_cb->start,
		 "Motion detector start callback is not populated");

	if (st->motion_detector_cb && st->motion_detector_cb->start) {
		st->motion_detector_cb->start();

		st->poll_state = MOTION_POLL_STATE_PASSIVE;
		__ASSERT_NO_MSG(!k_work_delayable_is_pending(&st->motion_poll_work));
		ret = k_work_schedule(&st->motion_poll_work, SEPARATED_UT_SAMPLING_RATE1);
		__ASSERT_NO_MSG(ret == 1);
	} else {
		LOG_ERR("Motion detector start callback is not populated");
	}
}

static void state_reset(struct md_state *st)
{
	int ret;

	st->poll_state = MOTION_POLL_STATE_STOPPED;
	st->sound_count = 0;

	ret = k_work_cancel_delayable(&st->motion_enable_work);
	__ASSERT_NO_MSG((!ret) || (ret == (K_WORK_RUNNING | K_WORK_CANCELING)));
	ret = k_work_cancel_delayable(&st->motion_poll_work);
	__ASSERT_NO_MSG((!ret) || (ret == (K_WORK_RUNNING | K_WORK_CANCELING)));
	ret = k_work_cancel_delayable(&st->motion_poll_duration_work);
	__ASSERT_NO_MSG((!ret) || (ret == (K_WORK_RUNNING | K_WORK_CANCELING)));
}

static void backoff_setup(struct md_state *st)
{
	int ret;

	LOG_DBG("Setting up motion detector backoff");

	state_reset(st);
	__ASSERT_NO_MSG(!k_work_delayable_is_pending(&st->motion_enable_work));
	ret = k_work_schedule(&st->motion_enable_work, backoff_period_get());
	__ASSERT_NO_MSG(ret == 1);
}

static void motion_detector_stop(struct md_state *st)
{
	__ASSERT(st->motion_detector_cb, "Motion detector callback structure is not registered");
	__ASSERT(st->motion_detector_cb->stop,
		 "Motion detector stop callback is not populated");

	if (st->motion_detector_cb && st->motion_detector_cb->stop) {
		st->motion_detector_cb->stop();
	} else {
		LOG_ERR("Motion detector stop callback is not populated");
	}
}

static void motion_poll_handle(struct md_state *st)
{
	bool motion_detected;

	__ASSERT(st->motion_detector_cb, "Motion detector callback structure is not registered");
	__ASSERT(st->motion_detector_cb->period_expired,
		 "Motion detector period_expired callback is not populated");

	if (!st->motion_detector_cb || !st->motion_detector_cb->period_expired) {
		LOG_ERR("Motion detector period_expired callback is not populated");
		return;
	}

	motion_detected = st->motion_detector_cb->period_expired();
	if (motion_detected) {
		__ASSERT_NO_MSG(sound_cb);
		sound_cb->sound_start(state_user(st));
		st->poll_state = (st->poll_state == MOTION_POLL_STATE_PASSIVE) ?
				 MOTION_POLL_STATE_PASSIVE_SOUND_REQUESTED :
				 MOTION_POLL_STATE_ACTIVE_SOUND_REQUESTED;
		st->sound_count++;

		if (st->sound_count >= SEPARATED_UT_MAX_SOUND_COUNT) {
			LOG_DBG("Stopping the motion detector: %d sounds played",
				SEPARATED_UT_MAX_SOUND_COUNT);

			motion_detector_stop(st);
			backoff_setup(st);
		}
	} else {
		(void) k_work_reschedule(&st->motion_poll_work,
					 (st->poll_state == MOTION_POLL_STATE_PASSIVE) ?
					 SEPARATED_UT_SAMPLING_RATE1 :
					 SEPARATED_UT_SAMPLING_RATE2);
	}
}

static void motion_poll_duration_work_handle(struct k_work *work)
{
	struct md_state *st = state_from_poll_duration_work(work);

	LOG_DBG("Stopping the motion detector: active poll duration timeout");

	motion_detector_stop(st);
	backoff_setup(st);
}

static void motion_poll_work_handle(struct k_work *work)
{
	struct md_state *st = state_from_poll_work(work);

	if (st->poll_state == MOTION_POLL_STATE_PASSIVE) {
		LOG_DBG("Passive motion polling");
		motion_poll_handle(st);
	} else if (st->poll_state == MOTION_POLL_STATE_ACTIVE) {
		LOG_DBG("Active motion polling");
		motion_poll_handle(st);
	} else {
		__ASSERT(0, "Invalid motion polling state");
	}
}

static void separated_mode_transition_handle(struct md_state *st)
{
	int err;
	int ret;
	uint16_t separated_ut_timeout_period_seed;
	uint32_t separated_ut_timeout_period;

	err = sys_csrand_get(&separated_ut_timeout_period_seed,
			     sizeof(separated_ut_timeout_period_seed));
	if (err) {
		LOG_WRN("DULT: sys_csrand_get failed: %d", err);
		sys_rand_get(&separated_ut_timeout_period_seed,
			     sizeof(separated_ut_timeout_period_seed));
	}

	uint32_t timeout_min = timeout_period_min_get();
	uint32_t timeout_max = timeout_period_max_get();
	uint32_t timeout_diff = timeout_max - timeout_min;

#if !CONFIG_DULT_MOTION_DETECTOR_TEST_MODE
	BUILD_ASSERT(SEPARATED_UT_TIMEOUT_PERIOD_DIFF < UINT16_MAX);
#else
	__ASSERT_NO_MSG(timeout_diff < UINT16_MAX);
#endif

	separated_ut_timeout_period = timeout_diff;
	separated_ut_timeout_period *= separated_ut_timeout_period_seed;
	separated_ut_timeout_period /= UINT16_MAX;
	separated_ut_timeout_period += timeout_min;

	LOG_DBG("Starting the work for enabling the motion detector. "
		"Randomized timeout set to: %" PRIu32 " minutes",
		separated_ut_timeout_period);
	__ASSERT_NO_MSG(!k_work_delayable_is_pending(&st->motion_enable_work));
	ret = k_work_schedule(&st->motion_enable_work,
			      K_MINUTES(separated_ut_timeout_period));
	__ASSERT_NO_MSG(ret == 1);
}

static void near_owner_mode_transition_handle(struct md_state *st)
{
	if (st->poll_state != MOTION_POLL_STATE_STOPPED) {
		LOG_DBG("Stopping the motion detector: owner nearby");
		motion_detector_stop(st);
	} else {
		LOG_DBG("Motion detector is not running: owner nearby");
	}
	state_reset(st);
}

static void near_owner_state_changed(const struct dult_user *user,
				     enum dult_near_owner_state_mode mode)
{
	size_t idx = dult_user_slot_idx(user);
	struct md_state *st;

	if (idx == DULT_USER_SLOT_NONE) {
		return;
	}

	st = &states[idx];
	if (!st->is_enabled) {
		return;
	}

	switch (mode) {
	case DULT_NEAR_OWNER_STATE_MODE_SEPARATED:
		separated_mode_transition_handle(st);
		break;

	case DULT_NEAR_OWNER_STATE_MODE_NEAR_OWNER:
		near_owner_mode_transition_handle(st);
		break;

	default:
		__ASSERT_NO_MSG(false);
		break;
	}
}

static struct dult_near_owner_state_cb near_owner_state_cb = {
	.state_changed = near_owner_state_changed,
};

static bool sound_requested(enum motion_poll_state state)
{
	return (state == MOTION_POLL_STATE_PASSIVE_SOUND_REQUESTED) ||
	       (state == MOTION_POLL_STATE_ACTIVE_SOUND_REQUESTED);
}

static void sound_completed_handle(struct md_state *st)
{
	if (sound_requested(st->poll_state)) {
		int ret;

		if (st->poll_state == MOTION_POLL_STATE_PASSIVE_SOUND_REQUESTED) {
			__ASSERT_NO_MSG(
				!k_work_delayable_is_pending(&st->motion_poll_duration_work));
			ret = k_work_schedule(&st->motion_poll_duration_work,
					      SEPARATED_UT_ACTIVE_POLL_DURATION);
			__ASSERT_NO_MSG(ret == 1);
		}

		__ASSERT_NO_MSG(!k_work_delayable_is_pending(&st->motion_poll_work));
		ret = k_work_schedule(&st->motion_poll_work, SEPARATED_UT_SAMPLING_RATE2);
		__ASSERT_NO_MSG(ret == 1);
		st->poll_state = MOTION_POLL_STATE_ACTIVE;
	}
}

void dult_motion_detector_sound_state_change_notify(const struct dult_user *user,
						    bool active, bool native)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return;
	}

	if (!active) {
		sound_completed_handle(&states[idx]);
	}
}

void dult_motion_detector_sound_cb_register(const struct dult_motion_detector_sound_cb *cb)
{
	__ASSERT(!sound_cb,
		 "DULT motion detector: sound callback already registered");
	__ASSERT(cb && cb->sound_start,
		 "DULT motion detector: input callback structure with invalid parameters");

	sound_cb = cb;
}

int dult_motion_detector_cb_register(const struct dult_user *user,
				     const struct dult_motion_detector_cb *cb)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	state_init();

	if (dult_user_is_ready(user)) {
		LOG_ERR("DULT Motion Detector: module must be disabled to register callbacks");
		return -EACCES;
	}

	if (!(user->accessory_capabilities &
	      BIT(DULT_ACCESSORY_CAPABILITY_MOTION_DETECTOR_UT_BIT_POS))) {
		LOG_ERR("DULT Motion Detector: motion detector capability must be declared to "
			"register callbacks");
		return -EINVAL;
	}

	if (states[idx].motion_detector_cb) {
		LOG_ERR("DULT Motion Detector: motion detector callbacks already registered");
		return -EALREADY;
	}

	if (!cb || !cb->start || !cb->period_expired || !cb->stop) {
		return -EINVAL;
	}

	states[idx].motion_detector_cb = cb;

	return 0;
}

int dult_motion_detector_enable(const struct dult_user *user)
{
	static bool dult_near_owner_state_cb_registered;
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	state_init();

	if (states[idx].is_enabled) {
		LOG_ERR("DULT Motion Detector: already enabled");
		return -EALREADY;
	}

	if (!states[idx].motion_detector_cb) {
		LOG_ERR("DULT Motion Detector: callbacks must be registered at this point");
		return -EINVAL;
	}

	if (!dult_near_owner_state_cb_registered) {
		dult_near_owner_state_cb_register(&near_owner_state_cb);
		dult_near_owner_state_cb_registered = true;
	}

	states[idx].is_enabled = true;

	near_owner_state_changed(user, dult_near_owner_state_get(user));

	return 0;
}

int dult_motion_detector_reset(const struct dult_user *user)
{
	size_t idx = dult_user_slot_idx(user);

	if (idx == DULT_USER_SLOT_NONE) {
		return -EACCES;
	}

	if (!states[idx].is_enabled) {
		LOG_ERR("DULT Motion Detector: is not enabled");
		return -EALREADY;
	}

	states[idx].is_enabled = false;

	if (states[idx].poll_state != MOTION_POLL_STATE_STOPPED) {
		motion_detector_stop(&states[idx]);
	}
	state_reset(&states[idx]);

	states[idx].motion_detector_cb = NULL;

	return 0;
}

#if CONFIG_DULT_MOTION_DETECTOR_TEST_MODE
int dult_test_mode_separated_ut_period_set(struct dult_test_mode_separated_ut_period data)
{
	if (data.timeout_max < data.timeout_min) {
		LOG_ERR("DULT Motion Detector test mode: "
			"timeout_max (%" PRIu32 ") must be >= timeout_min (%" PRIu32 ")",
			data.timeout_max, data.timeout_min);
		return -EINVAL;
	}

	ut_backoff_period_min = data.backoff;
	ut_timeout_period_min = data.timeout_min;
	ut_timeout_period_max = data.timeout_max;

	LOG_DBG("DULT Motion Detector test mode: backoff=%" PRIu32 " min, "
		"timeout=[%" PRIu32 ", %" PRIu32 "] min",
		data.backoff, data.timeout_min, data.timeout_max);

	return 0;
}
#endif /* CONFIG_DULT_MOTION_DETECTOR_TEST_MODE */
