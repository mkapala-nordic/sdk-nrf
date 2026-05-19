/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#include <zephyr/net_buf.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <dult/dult.h>
#include <dult/bt.h>

int dult_bt_adv_data_fill(const struct dult_user *user, struct bt_data *bt_adv_data,
			  uint8_t *buf, size_t buf_size, const struct dult_bt_adv_data *adv_data)
{
	struct net_buf_simple nb;
	size_t adv_data_len;
	uint8_t near_owner_state;

	if (!user || !bt_adv_data || !adv_data || !buf) {
		return -EINVAL;
	}

	/* Treat any zero-length proprietary payload as "no proprietary data",
	 * regardless of whether the buffer pointer is set. Only reject the
	 * inconsistent (buf == NULL, len != 0) case.
	 */
	if (!adv_data->proprietary.buf && adv_data->proprietary.len != 0) {
		return -EINVAL;
	}

	if (adv_data->proprietary.len > DULT_BT_ADV_PROPRIETARY_DATA_MAX_LEN) {
		return -EINVAL;
	}

	adv_data_len = DULT_BT_ADV_DATA_LEN(adv_data->proprietary.len);

	if (buf_size < adv_data_len) {
		return -EINVAL;
	}

	near_owner_state = adv_data->near_owner ?
			   BIT(DULT_BT_ADV_NEAR_OWNER_STATE_BIT_POS) : 0U;

	net_buf_simple_init_with_data(&nb, buf, buf_size);
	/* Reset the buffer to allow writing from offset 0. */
	net_buf_simple_reset(&nb);

	net_buf_simple_add_le16(&nb, DULT_BT_ADV_SVC_UUID);
	net_buf_simple_add_u8(&nb, user->network_id);
	net_buf_simple_add_u8(&nb, near_owner_state);

	/* Proprietary company payload data is OPTIONAL. */
	if (adv_data->proprietary.len > 0) {
		net_buf_simple_add_mem(&nb, adv_data->proprietary.buf, adv_data->proprietary.len);
	}

	bt_adv_data->type = BT_DATA_SVC_DATA16;
	bt_adv_data->data_len = adv_data_len;
	bt_adv_data->data = buf;

	return 0;
}
