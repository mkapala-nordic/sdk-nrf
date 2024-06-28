/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bluetooth/adv_prov.h>
#include <app_smp_adv_prov.h>

static bool ad_enabled = false;
static bool sd_enabled = false;

void app_smp_adv_prov_ad_enable(void)
{
	ad_enabled = true;
	sd_enabled = false;
}

void app_smp_adv_prov_sd_enable(void)
{
	ad_enabled = false;
	sd_enabled = true;
}

void app_smp_adv_prov_disable(void)
{
	ad_enabled = false;
	sd_enabled = false;
}

static void fill_data(struct bt_data *ad)
{
	static const uint8_t data[] = {
		0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86,
		0xd3, 0x4c, 0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d,
	};

	ad->type = BT_DATA_UUID128_ALL;
	ad->data_len = sizeof(data);
	ad->data = data;
}

static int get_ad_data(struct bt_data *ad, const struct bt_le_adv_prov_adv_state *state,
		    struct bt_le_adv_prov_feedback *fb)
{
	if (!ad_enabled) {
		return -ENOENT;
	}

	fill_data(ad);

	return 0;
}

static int get_sd_data(struct bt_data *sd, const struct bt_le_adv_prov_adv_state *state,
		    struct bt_le_adv_prov_feedback *fb)
{
	if (!sd_enabled) {
		return -ENOENT;
	}

	fill_data(sd);

	return 0;
}

/* Used in discoverable adv */
BT_LE_ADV_PROV_AD_PROVIDER_REGISTER(smp_ad, get_ad_data);

/* Used in the not-discoverable adv */
BT_LE_ADV_PROV_SD_PROVIDER_REGISTER(smp_sd, get_sd_data);
