/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "expression_emote.h"
#include "esp_mmap_assets.h"
#include "emote_types.h"
#include "emote_load.h"
#include "gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
typedef struct emote_s emote_t;

// Internal data structures
typedef struct {
    const void *data;
    size_t size;
    uint8_t fps;
    bool loop;
    mmap_assets_handle_t handle;
} emoji_data_t;

typedef struct {
    const void *data;
    size_t size;
    mmap_assets_handle_t handle;
} icon_data_t;

struct emote_s {
    bool is_initialized;

    gfx_handle_t gfx_emote_handle;

    gfx_obj_t *gfx_objects[EMOTE_OBJ_MAX];

    lv_font_t *gfx_font;

    bool battery_is_charging;
    int8_t battery_percent;

    assets_hash_table_t *emoji_data;
    assets_hash_table_t *icon_data;

    mmap_assets_handle_t boot_assets_handle;
    mmap_assets_handle_t emote_assets_handle;

    void *listen_anim_cache;
    void *emoji_anim_cache;

    void *emerg_dlg_cache;
    void *tips_icon_cache;
    void *charge_icon_cache;
    void *font_cache;
    void *boot_anim_cache;

    gfx_image_dsc_t tips_img_dsc;
    gfx_image_dsc_t charge_img_dsc;

    bool boot_anim_completed;

    gfx_timer_handle_t dialog_timer;

    emote_flush_ready_cb_t flush_cb;
};

#ifdef __cplusplus
}
#endif
