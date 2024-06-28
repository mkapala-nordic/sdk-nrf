/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_SMP_ADV_PROV_H_
#define APP_SMP_ADV_PROV_H_

#include <stdbool.h>

/**
 * @defgroup app_smp_adv_prov SMP advertising data provider API
 * @brief SMP advertising data provider API
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Due to using legacy advertising set size, add SMP UUID to either AD or SD,
 * depending on the space availability related to the provisioning state.
 * Otherwise, the advertising set size would be exceeded and the advertising
 * would not start.
 */
void app_smp_adv_prov_ad_enable(void);
void app_smp_adv_prov_sd_enable(void);
void app_smp_adv_prov_disable(void);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* APP_SMP_ADV_PROV_H_ */
