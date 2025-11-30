/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "expression_emote.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===== DEFAULT VALUES =====
#define EMOTE_INDEX_JSON_FILENAME       "index.json"
#define EMOTE_DEFAULT_FONT_SIZE         26
#define EMOTE_DEFAULT_SCROLL_SPEED      10
#define EMOTE_DEFAULT_LABEL_HEIGHT      25
#define EMOTE_DEFAULT_LABEL_WIDTH       100
#define EMOTE_DEFAULT_LABEL_Y_OFFSET    20
#define EMOTE_DEFAULT_ANIMATION_FPS     30
#define EMOTE_DEFAULT_FONT_COLOR        0xFFFFFF

// ===== ICON NAME CONSTANTS =====
#define EMOTE_ICON_MIC                  "icon_mic"
#define EMOTE_ICON_TIPS                 "icon_tips"
#define EMOTE_ICON_LISTEN               "listen"
#define EMOTE_ICON_SPEAKER              "icon_speaker"
#define EMOTE_ICON_BATTERY_BG           "battery_bg"
#define EMOTE_ICON_BATTERY_CHARGE       "battery_charge"

// ===== OBJECT TYPE ENUM =====
typedef enum {
    EMOTE_OBJ_ANIM_BOOT = 0,      // For boot animation playback
    EMOTE_OBJ_ANIM_EYE,           // For AI buddy eye expressions
    EMOTE_OBJ_ANIM_LISTEN,        // For listening indicator
    EMOTE_OBJ_ANIM_EMERG_DLG,     // For emergency dialog
    EMOTE_OBJ_ICON_STATUS,        // For status indicators
    EMOTE_OBJ_LABEL_TOAST,        // For text notifications
    EMOTE_OBJ_LABEL_CLOCK,        // For time display
    EMOTE_OBJ_TIMER_STATUS,       // For status timer
    EMOTE_OBJ_LABEL_BATTERY,      // For battery percent display
    EMOTE_OBJ_ICON_CHARGE,        // For battery charge icon
    EMOTE_OBJ_QRCODE,             // For QR code display
    EMOTE_OBJ_MAX                 // Boundary marker
} emote_obj_type_t;

// ===== UI ELEMENT NAME CONSTANTS =====
#define EMOTE_ELEMENT_BOOT_ANIM         "boot_anim"
#define EMOTE_ELEMENT_EYE_ANIM          "eye_anim"
#define EMOTE_ELEMENT_EMERG_DLG         "emerg_dlg"
#define EMOTE_ELEMENT_TOAST_LABEL       "toast_label"
#define EMOTE_ELEMENT_CLOCK_LABEL       "clock_label"
#define EMOTE_ELEMENT_LISTEN_ANIM       "listen_anim"
#define EMOTE_ELEMENT_STATUS_ICON       "status_icon"
#define EMOTE_ELEMENT_CHARGE_ICON       "charge_icon"
#define EMOTE_ELEMENT_BAT_LEFT_LABEL    "battery_label"
#define EMOTE_ELEMENT_TIMER_STATUS      "clock_timer"
#define EMOTE_ELEMENT_QRCODE            "qrcode"

#ifdef __cplusplus
}
#endif
