/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _DULT_BATTERY_H_
#define _DULT_BATTERY_H_

#include <stdint.h>
#include <stddef.h>

/**
 * @defgroup dult_battery Detecting Unwanted Location Trackers battery
 * @brief Private API for DULT specification implementation of the battery module
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <dult/dult.h>

/** Encode the battery type configuration.
 *  The configuration is device-wide (set via Kconfig).
 *
 * @return Byte with an encoded information about the battery type.
 */
uint8_t dult_battery_type_encode(void);

/** Encode the battery level configuration for the supplied user.
 *
 * @param user User structure used to authenticate the user.
 *
 * @return Byte with an encoded information about the battery level for @p user.
 */
uint8_t dult_battery_level_encode(const struct dult_user *user);

/** @brief Enable DULT battery for the supplied user.
 *
 *  @param user User structure used to authenticate the user.
 *
 *  @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int dult_battery_enable(const struct dult_user *user);

/** @brief Reset DULT battery for the supplied user.
 *
 *  @param user User structure used to authenticate the user.
 *
 *  @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int dult_battery_reset(const struct dult_user *user);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _DULT_BATTERY_H_ */
