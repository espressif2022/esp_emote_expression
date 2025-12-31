/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"
#include "expression_emote.h"
#include "esp_mmap_assets.h"
#include "gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
typedef struct assets_hash_table_s assets_hash_table_t;

// ===== DEFAULT VALUES =====
#define EMOTE_INDEX_JSON_FILENAME       "index.json"
#define EMOTE_DEF_SCROLL_SPEED          CONFIG_EMOTE_DEF_SCROLL_SPEED
#define EMOTE_DEF_LABEL_HEIGHT          CONFIG_EMOTE_DEF_LABEL_HEIGHT
#define EMOTE_DEF_LABEL_WIDTH           CONFIG_EMOTE_DEF_LABEL_WIDTH
#define EMOTE_DEF_LABEL_Y_OFFSET        CONFIG_EMOTE_DEF_LABEL_Y_OFFSET
#define EMOTE_DEF_ANIMATION_FPS         CONFIG_EMOTE_DEF_ANIMATION_FPS
#define EMOTE_DEF_FONT_COLOR            CONFIG_EMOTE_DEF_FONT_COLOR
#define EMOTE_DEF_BG_COLOR              CONFIG_EMOTE_DEF_BG_COLOR

// ===== ICON NAME CONSTANTS =====
#define EMOTE_ICON_MIC                  "icon_mic"
#define EMOTE_ICON_TIPS                 "icon_tips"
#define EMOTE_ICON_LISTEN               "listen"
#define EMOTE_ICON_SPEAKER              "icon_speaker"
#define EMOTE_ICON_BATTERY_BG           "battery_bg"
#define EMOTE_ICON_BATTERY_CHARGE       "battery_charge"

// ===== OBJECT TYPE ENUM =====
typedef enum {
    EMOTE_DEF_OBJ_ANIM_BOOT = 0,      // For boot animation playback
    EMOTE_DEF_OBJ_LEBAL_DEFAULT,      // For default label
    EMOTE_DEF_OBJ_ANIM_EYE,           // For AI buddy eye expressions
    EMOTE_DEF_OBJ_ANIM_LISTEN,        // For listening indicator
    EMOTE_DEF_OBJ_ANIM_EMERG_DLG,     // For emergency dialog
    EMOTE_DEF_OBJ_ICON_STATUS,        // For status indicators
    EMOTE_DEF_OBJ_LABEL_TOAST,        // For text notifications
    EMOTE_DEF_OBJ_LABEL_CLOCK,        // For time display
    EMOTE_DEF_OBJ_TIMER_STATUS,       // For status timer
    EMOTE_DEF_OBJ_LABEL_BATTERY,      // For battery percent display
    EMOTE_DEF_OBJ_ICON_CHARGE,        // For battery charge icon
    EMOTE_DEF_OBJ_QRCODE,             // For QR code display
    EMOTE_DEF_OBJ_MAX                 // Boundary marker
} emote_obj_type_t;

// ===== Type Definitions =====
typedef struct emote_custom_obj_entry_s {
    char *name;                    // Object name (dynamically allocated)
    gfx_obj_t *obj;                // Object pointer
    struct emote_custom_obj_entry_s *next;  // Next entry in linked list
} emote_custom_obj_entry_t;

struct emote_s {
    bool is_initialized;

    gfx_handle_t gfx_emote_handle;

    gfx_obj_t *def_objects[EMOTE_DEF_OBJ_MAX];

    emote_custom_obj_entry_t *custom_objects;  // Linked list for custom objects

    lv_font_t *gfx_font;

    bool bat_is_charging;
    int8_t battery_percent;

    assets_hash_table_t *emoji_table;
    assets_hash_table_t *icon_table;

    mmap_assets_handle_t assets_handle;

    void *boot_anim_cache; //animation
    void *listen_anim_cache; //animation
    void *emoji_anim_cache; //animation
    void *emerg_dlg_cache; //animation

    void *tips_icon_cache; //image
    void *charge_icon_cache; //image

    void *font_cache; //font

    gfx_image_dsc_t tips_img_dsc; //image
    gfx_image_dsc_t charge_img_dsc; //image

    gfx_timer_handle_t dialog_timer;

    emote_flush_ready_cb_t flush_cb;
    emote_update_cb_t update_cb;

    int h_res;
    int v_res;
};

#ifdef __cplusplus
}
#endif
