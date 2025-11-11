#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# Add remote project
ExternalZephyrProject_Add(
    APPLICATION remote
    SOURCE_DIR ${APP_DIR}/remote
    BOARD nrf54h20dk/nrf54h20/cpurad
    BOARD_REVISION ${BOARD_REVISION}
  )


ExternalZephyrProject_Add(
    APPLICATION radio_loader
    SOURCE_DIR "${ZEPHYR_NRF_MODULE_DIR}/samples/radio_loader"
    BOARD nrf54h20dk/nrf54h20/cpurad
    BOARD_REVISION ${BOARD_REVISION}
  )
  