/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _DULT_SOUND_H_
#define _DULT_SOUND_H_

#include <stdint.h>
#include <stddef.h>

/**
 * @defgroup dult_sound Detecting Unwanted Location Trackers sound
 * @brief Private API for DULT specification implementation of the sound module
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <dult/dult.h>

/** @brief Enable DULT sound for the supplied user.
 *
 *  @param user User structure used to authenticate the user.
 *
 *  @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int dult_sound_enable(const struct dult_user *user);

/** @brief Reset DULT sound for the supplied user.
 *
 *  @param user User structure used to authenticate the user.
 *
 *  @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int dult_sound_reset(const struct dult_user *user);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _DULT_SOUND_H_ */
