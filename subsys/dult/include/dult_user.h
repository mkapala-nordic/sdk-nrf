/*
 * Copyright (c) 2024-2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _DULT_USER_H_
#define _DULT_USER_H_

#include <stdint.h>
#include <stddef.h>

#include <zephyr/bluetooth/conn.h>

#include <dult/dult.h>

/**
 * @defgroup dult_user Detecting Unwanted Location Trackers user
 * @brief Private API for DULT specification implementation DULT user module
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum length of a string parameter in the DULT user structure */
#define DULT_USER_STR_PARAM_LEN_MAX	64

/** Sentinel slot index meaning "no slot". */
#define DULT_USER_SLOT_NONE		((size_t)-1)

/** @brief Check if provided DULT user is registered.
 *
 *  @param user	User structure to be checked.
 *
 *  @return True if the provided user is registered. Otherwise, False is returned.
 */
bool dult_user_is_registered(const struct dult_user *user);

/** @brief Get the slot index assigned to a registered DULT user.
 *
 *  Feature modules use the slot index to look up their own per-user state in a
 *  fixed-size array of @kconfig{CONFIG_DULT_USER_MAX} entries.
 *
 *  @param user User structure used to authenticate the user.
 *
 *  @return Slot index in range [0, @kconfig{CONFIG_DULT_USER_MAX}) if the user is
 *          registered, @ref DULT_USER_SLOT_NONE otherwise.
 */
size_t dult_user_slot_idx(const struct dult_user *user);

/** @brief Get the DULT user pointer registered in the given slot.
 *
 *  @param slot_idx Slot index in range [0, @kconfig{CONFIG_DULT_USER_MAX}).
 *
 *  @return Pointer to the DULT user registered in the slot, or NULL if the slot is empty
 *          or @p slot_idx is out of range.
 */
const struct dult_user *dult_user_by_slot_idx(size_t slot_idx);

/** @brief Get the ANOS callback structure registered by the supplied user.
 *
 *  @param user User structure used to authenticate the user.
 *
 *  @return Pointer to the ANOS callback structure, or NULL if none was registered or the
 *          user is not registered.
 */
const struct dult_anos_cb *dult_user_anos_cb_get(const struct dult_user *user);

/** @brief Resolve a DULT user for the supplied Bluetooth connection.
 *
 *  Iterates registered users and invokes each user's @ref dult_anos_cb.owns_connection
 *  callback. Returns the first user that claims @p conn.
 *
 *  When @kconfig{CONFIG_DULT_USER_MAX} equals 1, returns the single registered user as a
 *  fallback if it does not implement @c owns_connection or returns false. With more than
 *  one registered user, no fallback is applied.
 *
 *  @param conn Bluetooth connection.
 *
 *  @return Pointer to the resolved user, or NULL if no user owns the connection.
 */
const struct dult_user *dult_user_lookup_by_conn(struct bt_conn *conn);

/** @brief Iterator callback used by @ref dult_user_for_each. */
typedef void (*dult_user_iter_cb_t)(const struct dult_user *user, void *user_data);

/** @brief Invoke @p cb for every registered DULT user.
 *
 *  @param cb        Iterator callback.
 *  @param user_data Opaque pointer forwarded to @p cb.
 */
void dult_user_for_each(dult_user_iter_cb_t cb, void *user_data);

/** @brief Get the currently registered DULT user.
 *
 *  Backwards-compatible helper for single-user builds. Asserts when more than one user is
 *  registered and instead callers should pass an explicit @c user pointer.
 *
 *  @return Pointer to the only registered DULT user, or NULL if none is registered.
 */
const struct dult_user *dult_user_get(void);

/** @brief Check whether the supplied DULT user has been enabled with @ref dult_enable.
 *
 *  @param user User structure used to authenticate the user. When @c NULL, returns true
 *              if any registered user is enabled (single-user backwards-compatible form).
 *
 *  @return True if the user is enabled. Otherwise, false is returned.
 */
bool dult_user_is_ready(const struct dult_user *user);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _DULT_USER_H_ */
