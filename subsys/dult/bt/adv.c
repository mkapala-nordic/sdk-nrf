/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>

#include <zephyr/net_buf.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <dult.h>

#include "dult_near_owner_state.h"
#include "dult_user.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dult_bt_adv, CONFIG_DULT_LOG_LEVEL);

/** DULT Bluetooth advertising service UUID. */
#define DULT_BT_ADV_SVC_UUID		(0xFCB2)

#define DULT_NEAR_OWNER_STATE_BIT_POS	(0)
#define DULT_PROPRIETARY_DATA_MAX_LEN	(22)

static const uint16_t dult_bt_adv_svc_uuid = DULT_BT_ADV_SVC_UUID;

size_t dult_bt_adv_data_size(const struct dult_bt_adv_data *adv_data)
{
	size_t size = 0;

	size += sizeof(dult_bt_adv_svc_uuid);
	size += sizeof(uint8_t); /* Network ID */
	size += sizeof(uint8_t); /* Near-owner state */

	if (adv_data && adv_data->propertiary_buf && adv_data->len > 0) {
		if (adv_data->len > DULT_PROPRIETARY_DATA_MAX_LEN) {
			return SIZE_MAX;
		}

		size += adv_data->len;
	}

	return size;
}

int dult_bt_adv_data_fill(struct bt_data *bt_adv_data, uint8_t *buf, size_t buf_size,
	const struct dult_user *user, const struct dult_bt_adv_data *adv_data)
{
	struct net_buf_simple nb;
	size_t adv_data_len;
	enum dult_near_owner_state_mode mode;
	uint8_t near_owner_state = 0x0;

	if (!dult_user_is_registered(user)) {
		return -EACCES;
	}

	if (!bt_adv_data) {
		LOG_ERR("Bluetooth advertising data is invalid");
		return -EINVAL;
	}

	if (!buf || buf_size == 0) {
		LOG_ERR("Buffer is invalid");
		return -EINVAL;
	}

	adv_data_len = dult_bt_adv_data_size(adv_data);
	if (adv_data_len == SIZE_MAX) {
		LOG_ERR("Proprietary data length is too long");
		return -EINVAL;
	}

	if (adv_data_len > buf_size) {
		LOG_ERR("Buffer is too small");
		return -EINVAL;
	}

	mode = dult_near_owner_state_get();
	if (mode >= DULT_NEAR_OWNER_STATE_MODE_COUNT) {
		LOG_ERR("Invalid near-owner state mode");
		return -EINVAL;
	}

	WRITE_BIT(near_owner_state, 
		  DULT_NEAR_OWNER_STATE_BIT_POS, 
		  (mode == DULT_NEAR_OWNER_STATE_MODE_NEAR_OWNER));

	net_buf_simple_init_with_data(&nb, buf, buf_size);
	net_buf_simple_reset(&nb);

	net_buf_simple_add_u16(&nb, dult_bt_adv_svc_uuid);
	net_buf_simple_add_u8(&nb, user->network_id);
	net_buf_simple_add_u8(&nb, near_owner_state);

	/* Proprietary company payload data is OPTIONAL. */
	if (adv_data && adv_data->propertiary_buf && adv_data->len > 0) {
		net_buf_simple_add_mem(&nb, adv_data->propertiary_buf, adv_data->len);
	}

	bt_adv_data->type = BT_DATA_SVC_DATA16;
	bt_adv_data->data_len = adv_data_len;
	bt_adv_data->data = buf;

	return 0;
}
