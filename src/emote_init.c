/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "expression_emote.h"

#include <string.h>
#include <stdlib.h>

#define GFX_LOG_MODULE GFX_LOG_MODULE_CORE
#include "common/gfx_check.h"
#include "common/gfx_log_priv.h"
#include "emote_defs.h"
#include "emote_table.h"
#include "emote_layout.h"

static const char *TAG = "Expression_init";

static void emote_flush_cb_wrapper(gfx_display_t *disp, gfx_coord_t x1, gfx_coord_t y1,
                                   gfx_coord_t x2, gfx_coord_t y2, const void *data)
{
    emote_handle_t self = (emote_handle_t)gfx_display_get_user_data(disp);
    if (self && self->flush_cb) {
        self->flush_cb(x1, y1, x2, y2, data, self);
    }
}

static void emote_update_cb_wrapper(gfx_display_t *disp, gfx_display_event_t event, const void *obj)
{
    emote_handle_t self = (emote_handle_t)gfx_display_get_user_data(disp);
    if (!self) {
        return;
    }

    // Check if emergency dialog animation is done
    if (obj == self->def_objects[EMOTE_DEF_OBJ_ANIM_EMERG_DLG].obj &&
            event == GFX_DISPLAY_EVENT_ALL_FRAME_DONE) {
        if (self->emerg_dlg_done_event) {
            (void)gfx_platform_event_set(self->emerg_dlg_done_event, EMOTE_EMERG_DLG_DONE_BIT);
        }
    }

    if (self && self->update_cb) {
        self->update_cb(event, obj, self);
    }
}

emote_handle_t emote_init(const emote_config_t *config)
{
    gfx_err_t ret = GFX_OK;
    emote_handle_t handle = NULL;
    gfx_object_t *obj_default = NULL;

    GFX_GOTO_ON_FALSE(config, GFX_ERR_INVALID_ARG, error, TAG, "config is NULL");

    // Allocate handle
    handle = (emote_handle_t)calloc(1, sizeof(struct emote_s));
    GFX_GOTO_ON_FALSE(handle, GFX_ERR_NO_MEM, error, TAG, "Failed to allocate emote manager handle");

    memset(handle, 0, sizeof(struct emote_s));

    // Initialize bat_percent
    handle->bat_percent = -1;
    handle->flush_cb = config->flush_cb;
    handle->update_cb = config->update_cb;

    handle->h_res = config->gfx_emote.h_res;
    handle->v_res = config->gfx_emote.v_res;
    handle->user_data = config->user_data;

    gfx_core_config_t gfx_cfg = {
        .fps = config->gfx_emote.fps,
        .task = {
            .task_priority = config->task.task_priority,
            .task_stack = config->task.task_stack,
            .task_affinity = config->task.task_affinity,
            .task_stack_caps = config->task.task_stack_in_ext ?
            GFX_CORE_TASK_STACK_CAP_DEFAULT : (GFX_CORE_TASK_STACK_CAP_INTERNAL | GFX_CORE_TASK_STACK_CAP_DEFAULT),
        }
    };

    handle->gfx_handle = gfx_core_init(&gfx_cfg);
    GFX_GOTO_ON_FALSE(handle->gfx_handle, GFX_ERR_INVALID_STATE, error, TAG, "Failed to initialize emote_gfx");

    /* Add default display */
    const gfx_color_format_t color_format = config->flags.swap ?
                                            GFX_COLOR_FORMAT_RGB565_SWAPPED :
                                            GFX_COLOR_FORMAT_RGB565;
    const gfx_display_config_t disp_cfg = {
        .h_res = (uint32_t)config->gfx_emote.h_res,
        .v_res = (uint32_t)config->gfx_emote.v_res,
        .color_format = color_format,
        .backend = config->backend,
        .flush_cb = emote_flush_cb_wrapper,
        .update_cb = emote_update_cb_wrapper,
        .user_data = handle,
        .flags = {
            .buff_dma = config->flags.buff_dma,
            .buff_spiram = config->flags.buff_spiram,
            .double_buffer = config->flags.double_buffer,
            .full_frame = false,
        },
        .buffers = {
            .buf1 = NULL,
            .buf2 = NULL,
            .buf_pixels = config->buffers.buf_pixels,
        },
    };
    handle->gfx_disp = gfx_display_add(handle->gfx_handle, &disp_cfg);
    GFX_GOTO_ON_FALSE(handle->gfx_disp != NULL, GFX_FAIL, error, TAG, "Failed to add display");

    // Default set
    gfx_core_lock(handle->gfx_handle);
    gfx_display_set_bg_color(handle->gfx_disp, GFX_COLOR_HEX(EMOTE_DEF_BG_COLOR));

    obj_default = emote_create_obj_by_name(handle, EMT_DEF_ELEM_DEFAULT_LABEL);
    GFX_GOTO_ON_FALSE(obj_default, GFX_ERR_INVALID_STATE, error_unlock, TAG, "Failed to create default label");
    gfx_object_set_size(obj_default, handle->h_res, EMOTE_DEF_LABEL_HEIGHT);

    gfx_core_unlock(handle->gfx_handle);
    GFX_LOGI(TAG, "Create default label: [%p]", obj_default);
    handle->is_initialized = true;
    (void)ret;  // ret is used by GFX_GOTO_ON_FALSE macro but not returned by this function
    return handle;

error_unlock:
    if (handle && handle->gfx_handle) {
        gfx_core_unlock(handle->gfx_handle);
    }

error:
    if (handle) {
        if (handle->gfx_handle) {
            gfx_core_deinit(handle->gfx_handle);
            handle->gfx_handle = NULL;
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

    // Unload assets (this will cleanup hash tables, fonts, objects, custom objects created by load, etc.)
    emote_unload_assets(handle);

    // Unmount assets
    emote_unmount_assets(handle);

    // Cleanup default label
    gfx_object_t *obj_default = handle->def_objects[EMOTE_DEF_OBJ_LEBAL_DEFAULT].obj;
    if (obj_default) {
        gfx_object_delete(obj_default);
        handle->def_objects[EMOTE_DEF_OBJ_LEBAL_DEFAULT].obj = NULL;
    }

    // Deinit engine
    if (handle->gfx_handle) {
        gfx_core_deinit(handle->gfx_handle);
        handle->gfx_handle = NULL;
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

gfx_err_t emote_wait_emerg_dlg_done(emote_handle_t handle, uint32_t timeout_ms)
{
    if (!handle) {
        return GFX_ERR_INVALID_ARG;
    }

    if (!handle->emerg_dlg_done_event) {
        return GFX_ERR_INVALID_STATE;
    }

    {
        uint32_t timeout = (timeout_ms == 0U) ? GFX_PLATFORM_WAIT_FOREVER : timeout_ms;
        gfx_platform_event_bits_t bits = gfx_platform_event_wait(handle->emerg_dlg_done_event,
                EMOTE_EMERG_DLG_DONE_BIT, true, false, timeout);
        if ((bits & EMOTE_EMERG_DLG_DONE_BIT) != 0U) {
            return GFX_OK;
        }
    }

    return GFX_ERR_TIMEOUT;
}

void *emote_get_user_data(emote_handle_t handle)
{
    return handle ? handle->user_data : NULL;
}
