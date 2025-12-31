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
#include "esp_check.h"
#include "emote_defs.h"
#include "emote_assets.h"
#include "emote_layout.h"
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

    if (self && self->update_cb) {
        self->update_cb(event, obj, self);
    }

    if (event == GFX_PLAYER_EVENT_ALL_FRAME_DONE) {
        // if (self->def_objects[EMOTE_DEF_OBJ_ANIM_BOOT]) {
        // }
        ESP_LOGI(TAG, "All frame done: [%p]", obj);
    } else if (event == GFX_PLAYER_EVENT_IDLE) {
        ESP_LOGI(TAG, "Idle");
    }
}

emote_handle_t emote_init(const emote_config_t *config)
{
    esp_err_t ret = ESP_OK;
    emote_handle_t handle = NULL;
    gfx_obj_t *boot_obj = NULL;
    gfx_obj_t *obj_default = NULL;

    ESP_GOTO_ON_FALSE(config, ESP_ERR_INVALID_ARG, error, TAG, "config is NULL");

    // Allocate handle
    handle = (emote_handle_t)calloc(1, sizeof(struct emote_s));
    ESP_GOTO_ON_FALSE(handle, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate emote manager handle");

    memset(handle, 0, sizeof(struct emote_s));

    // Initialize hash tables
    handle->emoji_table = emote_assets_table_create("emoji");
    ESP_GOTO_ON_FALSE(handle->emoji_table, ESP_ERR_NO_MEM, error, TAG, "Failed to create emoji_table hash table");

    handle->icon_table = emote_assets_table_create("icon");
    ESP_GOTO_ON_FALSE(handle->icon_table, ESP_ERR_NO_MEM, error, TAG, "Failed to create icon_table hash table");

    // Initialize battery_percent
    handle->battery_percent = -1;
    handle->flush_cb = config->flush_cb;
    handle->h_res = config->gfx_emote.h_res;
    handle->v_res = config->gfx_emote.v_res;

    gfx_core_config_t gfx_cfg = {
        .flush_cb = emote_flush_cb_wrapper,
        .update_cb = emote_update_cb_wrapper,
        .user_data = handle,
        .flags = {
            .swap = config->flags.swap,
            .double_buffer = config->flags.double_buffer,
            .buff_dma = config->flags.buff_dma,
            .buff_spiram = config->flags.buff_spiram,
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
    ESP_GOTO_ON_FALSE(handle->gfx_emote_handle, ESP_ERR_INVALID_STATE, error, TAG, "Failed to initialize emote_gfx");

    // Default set
    gfx_emote_lock(handle->gfx_emote_handle);
    gfx_emote_set_bg_color(handle->gfx_emote_handle, GFX_COLOR_HEX(EMOTE_DEF_BG_COLOR));

    boot_obj = emote_create_obj_by_name(handle, EMT_DEF_ELEM_BOOT_ANIM);
    ESP_GOTO_ON_FALSE(boot_obj, ESP_ERR_INVALID_STATE, error_unlock, TAG, "Failed to create boot animation");
    gfx_obj_align(boot_obj, GFX_ALIGN_CENTER, 0, 0);

    obj_default = emote_create_obj_by_name(handle, EMT_DEF_ELEM_DEFAULT_LABEL);
    ESP_GOTO_ON_FALSE(obj_default, ESP_ERR_INVALID_STATE, error_unlock, TAG, "Failed to create default label");

    gfx_emote_unlock(handle->gfx_emote_handle);
    ESP_LOGI(TAG, "Create default objects: boot: [%p], label: [%p]", boot_obj, obj_default);
    handle->is_initialized = true;
    (void)ret;  // ret is used by ESP_GOTO_ON_FALSE macro but not returned by this function
    return handle;

error_unlock:
    if (handle && handle->gfx_emote_handle) {
        gfx_emote_unlock(handle->gfx_emote_handle);
    }

error:
    if (handle) {
        if (handle->gfx_emote_handle) {
            gfx_emote_deinit(handle->gfx_emote_handle);
            handle->gfx_emote_handle = NULL;
        }
        if (handle->emoji_table) {
            emote_assets_table_destroy(handle->emoji_table);
            handle->emoji_table = NULL;
        }
        if (handle->icon_table) {
            emote_assets_table_destroy(handle->icon_table);
            handle->icon_table = NULL;
        }
        free(handle);
    }
    return NULL;
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
        for (int i = 0; i < EMOTE_DEF_OBJ_MAX; i++) {
            if (handle->def_objects[i]) {
                if (i == EMOTE_DEF_OBJ_TIMER_STATUS) {
                    gfx_timer_delete(handle->gfx_emote_handle, (gfx_timer_handle_t)handle->def_objects[i]);
                } else {
                    gfx_obj_delete(handle->def_objects[i]);
                }
                handle->def_objects[i] = NULL;
            }
        }
        // Cleanup custom objects
        emote_custom_obj_entry_t *entry = handle->custom_objects;
        while (entry) {
            emote_custom_obj_entry_t *next = entry->next;
            if (entry->obj) {
                gfx_obj_delete(entry->obj);
            }
            if (entry->name) {
                free(entry->name);
            }
            free(entry);
            entry = next;
        }
        handle->custom_objects = NULL;
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
    if (handle->assets_handle) {
        mmap_assets_del(handle->assets_handle);
        handle->assets_handle = NULL;
    }

    // Cleanup font
    if (handle->gfx_font) {
        gfx_font_lv_delete(handle->gfx_font);
        handle->gfx_font = NULL;
    }

    // Cleanup hash tables
    if (handle->emoji_table) {
        emote_assets_table_destroy(handle->emoji_table);
        handle->emoji_table = NULL;
    }
    if (handle->icon_table) {
        emote_assets_table_destroy(handle->icon_table);
        handle->icon_table = NULL;
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
