/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _DULT_BT_H_
#define _DULT_BT_H_

#include <stdint.h>
#include <stddef.h>
#include <zephyr/bluetooth/bluetooth.h>

#include <dult/dult.h>

/**
 * @defgroup dult_bt Detecting Unwanted Location Trackers - Bluetooth
 * @brief Detecting Unwanted Location Trackers specification implementation Bluetooth API
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** DULT Bluetooth advertising service UUID. */
#define DULT_BT_ADV_SVC_UUID		(0xFCB2)

/** @cond INTERNAL_HIDDEN */
#define DULT_BT_ADV_UUID_LEN				(2)
#define DULT_BT_ADV_NETWORK_ID_LEN			(1)
#define DULT_BT_ADV_NEAR_OWNER_STATE_LEN		(1)
#define DULT_BT_ADV_NEAR_OWNER_STATE_BIT_POS		(0)

#define DULT_BT_ADV_FIXED_LEN				\
	(DULT_BT_ADV_UUID_LEN + DULT_BT_ADV_NETWORK_ID_LEN + DULT_BT_ADV_NEAR_OWNER_STATE_LEN)

#define DULT_BT_ADV_PROPRIETARY_DATA_SPEC_MAX_LEN	(22)
#define DULT_BT_ADV_FLAGS_TLV_LEN			(3)
/** @endcond */

/** Maximum length of the proprietary data that can be placed in the DULT Bluetooth
 *  advertising data in bytes.
 *
 *  The proprietary data length is increased by the size of the optional flags TLV.
 */
#define DULT_BT_ADV_PROPRIETARY_DATA_MAX_LEN		\
	(DULT_BT_ADV_PROPRIETARY_DATA_SPEC_MAX_LEN + DULT_BT_ADV_FLAGS_TLV_LEN)

/** Calculate the length of the DULT Bluetooth advertising data.
 *
 *  @param[in] _proprietary_len Length of the proprietary data in bytes.
 *
 *  @return Length of the DULT Bluetooth advertising data in bytes.
 */
#define DULT_BT_ADV_DATA_LEN(_proprietary_len)		\
	(DULT_BT_ADV_FIXED_LEN + (_proprietary_len))

/** Maximum length of the DULT Bluetooth advertising data in bytes. */
#define DULT_BT_ADV_DATA_MAX_LEN			\
	(DULT_BT_ADV_DATA_LEN(DULT_BT_ADV_PROPRIETARY_DATA_MAX_LEN))

/** DULT Bluetooth advertising data. */
struct dult_bt_adv_data {
	/** Near-owner state. */
	bool near_owner;

	/** Network-specific proprietary data. */
	struct {
		/** Network-specific proprietary data buffer. */
		uint8_t *buf;

		/** Length of the network-specific proprietary data buffer.
		 *
		 *  The maximum length of the buffer is defined by the
		 *  @ref DULT_BT_ADV_PROPRIETARY_DATA_MAX_LEN macro.
		 */
		size_t len;
	} proprietary;
};

/** @brief Initialize the DULT Bluetooth advertising data structure.
 *
 *  @param[in] _near_owner	Near-owner state.
 *  @param[in] _proprietary_buf	Network-specific proprietary data buffer.
 *				Must be an array.
 *
 *  @return Initialized DULT Bluetooth advertising data structure.
 */
#define DULT_BT_ADV_DATA_INIT(_near_owner, _proprietary_buf)	\
	{								\
		.near_owner = (_near_owner),				\
		.proprietary = {					\
			.buf = (_proprietary_buf),			\
			.len = (ARRAY_SIZE(_proprietary_buf)),		\
		},							\
	}

/** @brief Encode the DULT location-enabled Bluetooth advertising payload.
 *
 *  Serializes the DULT data (UUID, network ID, near-owner byte, and optional
 *  proprietary data from @p adv_data) into @p buf and populates @p bt_adv_data to
 *  reference it. The caller must ensure that @p buf remains valid for as long as
 *  @p bt_adv_data is in use.
 *
 *  The function is stateless. The @p user structure is used only to read the network ID field;
 *  no DULT registration validation is performed.
 *
 *  @param[in] user	Pointer to the DULT user structure.
 *  @param[out] bt_adv_data Pointer to the Bluetooth advertising data structure to populate.
 *  @param[out] buf	Backing buffer for the serialized payload. Must be at least
 *			``DULT_BT_ADV_DATA_LEN(adv_data->proprietary.len)`` bytes.
 *  @param[in] buf_size	Size of @p buf in bytes.
 *  @param[in] adv_data	Advertising data to encode.
 *
 *  @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int dult_bt_adv_data_fill(const struct dult_user *user, struct bt_data *bt_adv_data,
			  uint8_t *buf, size_t buf_size, const struct dult_bt_adv_data *adv_data);

/** DULT Accessory Non-Owner Service (ANOS) opcode groups. */
enum dult_anos_groups {
	DULT_ANOS_GROUP_ACCESSORY_INFO,
	DULT_ANOS_GROUP_NON_OWNER_CONTROL,
};

/** Result of an ANOS opcode access check. */
enum dult_anos_access_result {
	/** Access is granted; dispatch the opcode normally. */
	DULT_ANOS_ACCESS_GRANTED,

	/** Access denied; reply with Invalid_state. */
	DULT_ANOS_ACCESS_DENIED_INVALID_STATE,

	/** Access denied; reply with Invalid_command. */
	DULT_ANOS_ACCESS_DENIED_INVALID_COMMAND,
};

/** DULT Accessory Non-Owner Service (ANOS) callback structure. */
struct dult_anos_cb {
	/** @brief Verify access to an ANOS opcode.
	 *
	 *  This callback is called to verify access to an ANOS opcode.
	 *
	 *  @param[in] conn Pointer to the Bluetooth connection.
	 *  @param[in] group ANOS opcode group.
	 *
	 *  @return Result of the access check.
	 */
	enum dult_anos_access_result (*verify_access)(struct bt_conn *conn,
						      enum dult_anos_groups group);

	/** @brief Check whether this DULT user owns the given Bluetooth connection.
	 *
	 *  Called by the ANOS GATT layer to resolve which registered DULT user should answer
	 *  a non-owner read/write on the supplied @p conn. Typical implementations compare
	 *  the connection's local Bluetooth identity (see @ref bt_conn_get_info) against the
	 *  identity used by the network's advertising set. Only the user whose callback
	 *  returns @c true on a given @p conn is considered for that operation.
	 *
	 *  When @kconfig{CONFIG_DULT_USER_MAX} equals 1 (single-user build), this callback is
	 *  optional. If it is @c NULL or no registered user claims the connection, the lone
	 *  registered user is used as a fallback. With more than one user registered, every
	 *  user must implement this callback so the routing decision is unambiguous.
	 *
	 *  @param[in] conn Pointer to the Bluetooth connection.
	 *
	 *  @return @c true if this user owns the connection, @c false otherwise.
	 */
	bool (*owns_connection)(struct bt_conn *conn);
};

/** @brief Register DULT Accessory Non-Owner Service (ANOS) callback structure.
 *
 *  Each registered DULT user installs its own ANOS callback structure. The callback is
 *  consulted by the ANOS GATT layer on every non-owner write to route the request to the
 *  user that owns the originating connection and to apply that user's access policy.
 *
 *  @param user User structure used to authenticate the user.
 *  @param cb   ANOS callback structure.
 *
 *  @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int dult_anos_cb_register(const struct dult_user *user, const struct dult_anos_cb *cb);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _DULT_BT_H_ */
