/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "emote_init.h"

// Forward declaration for cJSON (only pointer types used in header)
typedef struct cJSON cJSON;

#ifdef __cplusplus
extern "C" {
#endif

// ===== Boot Animation Functions =====
/**
 * @brief  Load and setup boot animation
 *
 * @param[in]  handle     Emote handle
 * @param[in]  anim_data  Boot animation data buffer
 * @param[in]  anim_size  Size of animation data
 *
 * @return
 *       - true   On success
 *       - false  Fail to setup boot animation
 */
bool emote_setup_boot_anim(emote_handle_t handle, uint8_t *anim_data, size_t anim_size);

/**
 * @brief  Delete boot animation and set system message
 *
 * @param[in]  handle  Emote handle
 */
void emote_delete_boot_anim(emote_handle_t handle);

// ===== Layout Application Functions =====
/**
 * @brief  Apply font configuration to emote system
 *
 * @param[in]  handle    Emote handle
 * @param[in]  fontData  Font data buffer
 *
 * @return
 *       - true   On success
 *       - false  Fail to apply fonts
 */
bool emote_apply_fonts(emote_handle_t handle, const uint8_t *fontData);

/**
 * @brief  Apply label layout configuration from JSON
 *
 * @param[in]  handle  Emote handle
 * @param[in]  name    Label element name
 * @param[in]  label   JSON object containing label configuration
 *
 * @return
 *       - true   On success
 *       - false  Fail to apply label layout
 */
bool emote_apply_label_layout(emote_handle_t handle, const char *name, cJSON *label);

/**
 * @brief  Apply image layout configuration from JSON
 *
 * @param[in]  handle  Emote handle
 * @param[in]  name    Image element name
 * @param[in]  image   JSON object containing image configuration
 *
 * @return
 *       - true   On success
 *       - false  Fail to apply image layout
 */
bool emote_apply_image_layout(emote_handle_t handle, const char *name, cJSON *image);

/**
 * @brief  Apply timer layout configuration from JSON
 *
 * @param[in]  handle  Emote handle
 * @param[in]  name    Timer element name
 * @param[in]  timer   JSON object containing timer configuration
 *
 * @return
 *       - true   On success
 *       - false  Fail to apply timer layout
 */
bool emote_apply_timer_layout(emote_handle_t handle, const char *name, cJSON *timer);

/**
 * @brief  Apply animation layout configuration from JSON
 *
 * @param[in]  handle     Emote handle
 * @param[in]  name       Animation element name
 * @param[in]  animation  JSON object containing animation configuration
 *
 * @return
 *       - true   On success
 *       - false  Fail to apply animation layout
 */
bool emote_apply_anim_layout(emote_handle_t handle, const char *name, cJSON *animation);

/**
 * @brief  Apply QRCode layout configuration from JSON
 *
 * @param[in]  handle  Emote handle
 * @param[in]  name    QRCode element name
 * @param[in]  qrcode  JSON object containing QRCode configuration
 *
 * @return
 *       - true   On success
 *       - false  Fail to apply QRCode layout
 */
bool emote_apply_qrcode_layout(emote_handle_t handle, const char *name, cJSON *qrcode);

// ===== UI Operation Functions =====
/**
 * @brief  Update clock label with current time
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
