/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "expression_emote.h"

#include <string.h>
#include <stdlib.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "emote_init.h"
#include "widget/gfx_font_lvgl.h"

static const char *TAG = "ExpressionEmote";

void emote_flush_cb_wrapper(gfx_handle_t handle, int x1, int y1, int x2, int y2, const void *data)
{
    emote_handle_t self = (emote_handle_t)gfx_emote_get_user_data(handle);
    if (self && self->flush_cb) {
        self->flush_cb(x1, y1, x2, y2, data, self);
    }
}

void emote_update_cb_wrapper(gfx_handle_t handle, gfx_player_event_t event, const void *obj)
{
    emote_handle_t self = (emote_handle_t)gfx_emote_get_user_data(handle);
    if (!self) {
        return;
    }

    if (event == GFX_PLAYER_EVENT_ALL_FRAME_DONE) {
        if (self->gfx_objects[EMOTE_OBJ_ANIM_BOOT]) {
            ESP_LOGI(TAG, "Boot animation completed");
            self->boot_anim_completed = true;
        }
    } else if (event == GFX_PLAYER_EVENT_IDLE) {
        ESP_LOGI(TAG, "Idle");
    }
}

emote_handle_t emote_init(const emote_config_t *config)
{
    ESP_LOGI(TAG, "EmoteManager init");
    if (!config) {
        return NULL;
    }

    // Allocate handle
    emote_handle_t handle = (emote_handle_t)calloc(1, sizeof(struct emote_s));
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate emote manager handle");
        return NULL;
    }

    memset(handle, 0, sizeof(struct emote_s));

    // Initialize hash tables
    handle->emoji_data = emote_assets_table_create();
    if (!handle->emoji_data) {
        ESP_LOGE(TAG, "Failed to create emoji_data hash table");
        free(handle);
        return NULL;
    }
    handle->icon_data = emote_assets_table_create();
    if (!handle->icon_data) {
        ESP_LOGE(TAG, "Failed to create icon_data hash table");
        emote_assets_table_destroy(handle->emoji_data);
        free(handle);
        return NULL;
    }

    // Initialize battery_percent
    handle->battery_percent = -1;

    handle->flush_cb = config->flush_cb;

    gfx_core_config_t gfx_cfg = {
        .flush_cb = emote_flush_cb_wrapper,
        .update_cb = emote_update_cb_wrapper,
        .user_data = handle,
        .flags = {
            .swap = config->flags.swap,
            .double_buffer = config->flags.double_buffer,
            .buff_dma = config->flags.buff_dma,
            .buff_spiram = 0,
        },
        .h_res = (uint32_t)config->gfx_emote.h_res,
        .v_res = (uint32_t)config->gfx_emote.v_res,
        .fps = (uint32_t)config->gfx_emote.fps,
        .buffers = {
            .buf1 = NULL,
            .buf2 = NULL,
            .buf_pixels = config->buffers.buf_pixels,
        },
        .task = {
            .task_priority = config->task.task_priority,
            .task_stack = config->task.task_stack,
            .task_affinity = config->task.task_affinity,
            .task_stack_caps = MALLOC_CAP_DEFAULT,
        }
    };

    handle->gfx_emote_handle = gfx_emote_init(&gfx_cfg);
    if (!handle->gfx_emote_handle) {
        ESP_LOGE(TAG, "Failed to initialize emote_gfx");
        emote_assets_table_destroy(handle->emoji_data);
        emote_assets_table_destroy(handle->icon_data);
        free(handle);
        return NULL;
    }

    handle->is_initialized = true;
    return handle;
}

bool emote_deinit(emote_handle_t handle)
{
    if (!handle) {
        return false;
    }

    if (!handle->is_initialized) {
        return true;
    }

    // Cleanup objects
    if (handle->gfx_emote_handle) {
        gfx_emote_lock(handle->gfx_emote_handle);
        for (int i = 0; i < EMOTE_OBJ_MAX; i++) {
            if (handle->gfx_objects[i]) {
                if (i == EMOTE_OBJ_TIMER_STATUS) {
                    gfx_timer_delete(handle->gfx_emote_handle, (gfx_timer_handle_t)handle->gfx_objects[i]);
                } else {
                    gfx_obj_delete(handle->gfx_objects[i]);
                }
                handle->gfx_objects[i] = NULL;
            }
        }
        gfx_emote_unlock(handle->gfx_emote_handle);
    }

    // Deinit engine
    if (handle->gfx_emote_handle) {
        gfx_emote_deinit(handle->gfx_emote_handle);
        handle->gfx_emote_handle = NULL;
    }

    // Release asset buffers
    if (handle->listen_anim_cache) {
        free(handle->listen_anim_cache);
        handle->listen_anim_cache = NULL;
    }
    if (handle->emoji_anim_cache) {
        free(handle->emoji_anim_cache);
        handle->emoji_anim_cache = NULL;
    }
    if (handle->emerg_dlg_cache) {
        free(handle->emerg_dlg_cache);
        handle->emerg_dlg_cache = NULL;
    }
    if (handle->tips_icon_cache) {
        free(handle->tips_icon_cache);
        handle->tips_icon_cache = NULL;
    }
    if (handle->charge_icon_cache) {
        free(handle->charge_icon_cache);
        handle->charge_icon_cache = NULL;
    }
    if (handle->font_cache) {
        free(handle->font_cache);
        handle->font_cache = NULL;
    }
    if (handle->boot_anim_cache) {
        free(handle->boot_anim_cache);
        handle->boot_anim_cache = NULL;
    }

    // Cleanup assets
    if (handle->emote_assets_handle) {
        mmap_assets_del(handle->emote_assets_handle);
        handle->emote_assets_handle = NULL;
    }
    if (handle->boot_assets_handle) {
        mmap_assets_del(handle->boot_assets_handle);
        handle->boot_assets_handle = NULL;
    }

    // Cleanup font
    if (handle->gfx_font) {
        gfx_font_lv_delete(handle->gfx_font);
        handle->gfx_font = NULL;
    }

    // Cleanup hash tables
    if (handle->emoji_data) {
        emote_assets_table_destroy(handle->emoji_data);
        handle->emoji_data = NULL;
    }
    if (handle->icon_data) {
        emote_assets_table_destroy(handle->icon_data);
        handle->icon_data = NULL;
    }

    // Cleanup emergency dialog timer
    if (handle->dialog_timer && handle->gfx_emote_handle) {
        gfx_timer_delete(handle->gfx_emote_handle, handle->dialog_timer);
        handle->dialog_timer = NULL;
    }

    handle->is_initialized = false;

    // Free handle memory
    free(handle);
    return true;
}

bool emote_is_initialized(emote_handle_t handle)
{
    return handle && handle->is_initialized;
}
