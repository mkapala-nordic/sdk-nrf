/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _DULT_TEST_H_
#define _DULT_TEST_H_

#include <stdint.h>

/**
 * @defgroup dult_test Detecting Unwanted Location Trackers - test functionality
 * @brief Test API for the Detecting Unwanted Location Trackers module
 *
 * This header collects all DULT test-mode APIs. Enabling
 * @kconfig{CONFIG_DULT_TEST} is required to make any per-feature test
 * Kconfig option (for example @kconfig{CONFIG_DULT_TEST_MOTION_DETECTOR}
 * for the motion-detector test APIs) selectable, but it is NOT sufficient
 * on its own: every API in this group requires its specific per-feature
 * Kconfig option to be enabled and will return ``-ENOTSUP`` at runtime
 * otherwise. Each function documents its specific per-feature Kconfig
 * dependency in its own description.
 *
 * The APIs in this group are not fully thread-safe and should be called
 * from cooperative thread context.
 *
 * @note Test-mode APIs are intended for qualification and debug use only
 *       and MUST NOT be enabled in production builds.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Separated UT timing parameters for the motion detector test functionality.
 *  All time values are in seconds.
 */
struct dult_test_motion_detector_separated_ut_period {
	/** Backoff period in seconds. */
	uint32_t backoff;

	/** Minimum time in separated state before enabling the motion detector, in seconds. */
	uint32_t timeout_min;

	/** Maximum time in separated state before enabling the motion detector, in seconds.
	 *  Must be greater than or equal to @ref timeout_min.
	 */
	uint32_t timeout_max;
};

/** @brief Override the DULT motion detector separated UT timing parameters.
 *
 *  Replaces the compile-time Kconfig defaults with the supplied runtime values.
 *  The new values take effect on the next timer arm; any already-running timer
 *  is not restarted.
 *
 *  This API can only be used when the @kconfig{CONFIG_DULT_TEST_MOTION_DETECTOR}
 *  Kconfig option is enabled.
 *
 *  @param data New timing parameters, in seconds. Each field must not exceed
 *              ``UINT32_MAX / MSEC_PER_SEC`` seconds.
 *              @ref dult_test_motion_detector_separated_ut_period.timeout_max
 *              must be greater than or equal to
 *              @ref dult_test_motion_detector_separated_ut_period.timeout_min.
 *
 *  @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int dult_test_motion_detector_separated_ut_period_set(
	struct dult_test_motion_detector_separated_ut_period data);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _DULT_TEST_H_ */
