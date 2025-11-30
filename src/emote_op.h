/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @deprecated This header is deprecated. Functions have been moved to emote_setup.h
 * Please include emote_setup.h instead and update your includes.
 * This file is kept for backward compatibility only.
 */

#pragma once

#include "expression_emote.h"

#ifdef __cplusplus
extern "C" {
#endif

// These functions are now declared in emote_setup.h
// Kept here for backward compatibility only
/**
 * @brief  Update clock label with current time
 *
 * @deprecated  Use emote_setup.h instead
 *
 * @param[in]  handle  Emote handle
 *
 * @return
 *       - true   On success
 *       - false  Fail to update clock label
 */
bool emote_set_label_clock(emote_handle_t handle);

/**
 * @brief  Update battery status display (percentage and charging icon)
 *
 * @deprecated  Use emote_setup.h instead
 *
 * @param[in]  handle  Emote handle
 *
 * @return
 *       - true   On success
 *       - false  Fail to update battery status
 */
bool emote_set_bat_status(emote_handle_t handle);

#ifdef __cplusplus
}
#endif
