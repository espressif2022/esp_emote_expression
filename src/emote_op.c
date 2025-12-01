/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "expression_emote.h"
#include "esp_mmap_assets.h"

#include "emote_op.h"
#include "emote_init.h"
#include "emote_load.h"
#include "emote_types.h"

static const char *TAG = "ExpressionEmote";

// Helper macro to hide object if exists
#define HIDE_OBJ(handle, obj_type) do { \
    gfx_obj_t *obj = (handle)->gfx_objects[obj_type]; \
    if (obj) gfx_obj_set_visible(obj, false); \
} while(0)

#define SHOW_OBJ(handle, obj_type) do { \
    gfx_obj_t *obj = (handle)->gfx_objects[obj_type]; \
    if (obj) gfx_obj_set_visible(obj, true); \
} while(0)

// ===== Event Handler Definitions =====
typedef bool (*emote_event_handler_t)(emote_handle_t handle, const char *message);

// Event handler lookup table entry
typedef struct {
    const char *event_name;
    emote_event_handler_t handler;
    bool skip_hide_ui;
} emote_event_entry_t;

static bool emote_handle_idle_event(emote_handle_t handle, const char *message);
static bool emote_handle_listen_event(emote_handle_t handle, const char *message);
static bool emote_handle_speak_event(emote_handle_t handle, const char *message);
static bool emote_handle_sys_set_event(emote_handle_t handle, const char *message);
static bool emote_handle_bat_event(emote_handle_t handle, const char *message);
static bool emote_handle_qrcode_set_event(emote_handle_t handle, const char *message);

static bool emote_set_image_tips(emote_handle_t handle, const char *name);
static bool emote_set_image_charging(emote_handle_t handle, const char *name, bool is_charging);
static bool emote_set_anim_listen(emote_handle_t handle, const char *name);
static bool emote_set_label_toast(emote_handle_t handle, const char *text);
static bool emote_set_label_battery(emote_handle_t handle, const char *text);

// Event handler lookup table
static const emote_event_entry_t event_table[] = {
    { EMOTE_MGR_EVT_IDLE,   emote_handle_idle_event,    false },
    { EMOTE_MGR_EVT_LISTEN, emote_handle_listen_event,  false },
    { EMOTE_MGR_EVT_SPEAK,  emote_handle_speak_event,   false },
    { EMOTE_MGR_EVT_SYS,    emote_handle_sys_set_event, false },
    { EMOTE_MGR_EVT_SET,    emote_handle_sys_set_event, false },
    { EMOTE_MGR_EVT_QRCODE, emote_handle_qrcode_set_event, false },
    { EMOTE_MGR_EVT_BAT,    emote_handle_bat_event,     true  },
};

#define EVENT_TABLE_SIZE (sizeof(event_table) / sizeof(event_table[0]))

// Event handler implementations
static bool emote_handle_idle_event(emote_handle_t handle, const char *message)
{
    (void)message;
    emote_set_bat_status(handle);
    emote_set_label_clock(handle);
    return true;
}

static bool emote_handle_listen_event(emote_handle_t handle, const char *message)
{
    emote_set_anim_listen(handle, EMOTE_ICON_LISTEN);
    emote_set_image_tips(handle, EMOTE_ICON_MIC);
    return true;
}

static bool emote_handle_speak_event(emote_handle_t handle, const char *message)
{
    emote_set_label_toast(handle, message);
    emote_set_image_tips(handle, EMOTE_ICON_SPEAKER);
    gfx_obj_t *obj = handle->gfx_objects[EMOTE_OBJ_LABEL_TOAST];
    if (obj) {
        gfx_label_set_snap_loop(obj, false);
    }
    return true;
}

static bool emote_handle_sys_set_event(emote_handle_t handle, const char *message)
{
    emote_set_label_toast(handle, message);
    emote_set_image_tips(handle, EMOTE_ICON_TIPS);
    gfx_obj_t *obj = handle->gfx_objects[EMOTE_OBJ_LABEL_TOAST];
    if (obj) {
        gfx_label_set_snap_loop(obj, true);
    }
    return true;
}

static bool emote_handle_qrcode_set_event(emote_handle_t handle, const char *message)
{
    ESP_LOGI(TAG, "handle_qrcode_set_event: %s", message);
    emote_set_label_toast(handle, message);
    emote_set_image_tips(handle, EMOTE_ICON_TIPS);
    HIDE_OBJ(handle, EMOTE_OBJ_ANIM_EYE);
    gfx_obj_t *obj = handle->gfx_objects[EMOTE_OBJ_LABEL_TOAST];
    if (obj) {
        gfx_label_set_snap_loop(obj, true);
    }
    return true;
}

static bool emote_handle_bat_event(emote_handle_t handle, const char *message)
{
    if (!message) {
        return false;
    }
    // message format: "charging,percent" e.g. "1,75" or "0,30"
    char *comma_pos = strchr(message, ',');
    if (comma_pos) {
        int percent = atoi(comma_pos + 1);
        handle->battery_is_charging = (message[0] == '1');
        handle->battery_percent = (percent < 0) ? 0 : (percent > 100 ? 100 : percent);
    }
    return true;
}

// Helper function to set icon image
static bool emote_set_icon_image(emote_handle_t handle, const char *name,
                                 emote_obj_type_t obj_type, void **cache_ptr, gfx_image_dsc_t *img_dsc, bool visible)
{
    if (!handle || !name) {
        return false;
    }

    icon_data_t *icon = NULL;
    if (!emote_find_data_by_key(handle, handle->icon_data, name, (void **)&icon)) {
        return false;
    }

    gfx_obj_t *obj = handle->gfx_objects[obj_type];
    if (!obj) {
        ESP_LOGE(TAG, "object not found");
        return false;
    }

    if (!icon->data) {
        ESP_LOGE(TAG, "icon.data is null");
        return false;
    }

    gfx_emote_lock(handle->gfx_emote_handle);
    const void *src_data = emote_acquire_data(handle, icon->handle, icon->data, icon->size, cache_ptr);
    if (!src_data) {
        ESP_LOGE(TAG, "Failed to acquire icon data");
        gfx_emote_unlock(handle->gfx_emote_handle);
        return false;
    }

    memcpy(&img_dsc->header, src_data, sizeof(gfx_image_header_t));
    img_dsc->data = (const uint8_t *)src_data + sizeof(gfx_image_header_t);
    img_dsc->data_size = icon->size - sizeof(gfx_image_header_t);

    gfx_img_set_src(obj, img_dsc);
    gfx_obj_set_visible(obj, visible);
    gfx_emote_unlock(handle->gfx_emote_handle);
    return true;
}

// Helper function to set animation from icon data
static bool emote_set_icon_animation(emote_handle_t handle, const char *name,
                                     emote_obj_type_t obj_type,
                                     void **cache_ptr, uint8_t fps, bool loop)
{
    if (!handle || !name) {
        return false;
    }

    icon_data_t *icon = NULL;
    if (!emote_find_data_by_key(handle, handle->icon_data, name, (void **)&icon)) {
        return false;
    }

    gfx_obj_t *obj = handle->gfx_objects[obj_type];
    if (!obj) {
        return false;
    }

    gfx_emote_lock(handle->gfx_emote_handle);
    const void *src_data = emote_acquire_data(handle, icon->handle, icon->data, icon->size, cache_ptr);
    if (!src_data) {
        ESP_LOGE(TAG, "Failed to acquire animation data");
        gfx_emote_unlock(handle->gfx_emote_handle);
        return false;
    }

    gfx_anim_set_src(obj, src_data, icon->size);
    gfx_anim_set_segment(obj, 0, 0xFFFF, fps, loop);
    gfx_anim_start(obj);
    gfx_obj_set_visible(obj, true);
    gfx_emote_unlock(handle->gfx_emote_handle);
    return true;
}

// Helper function to set label text
static bool emote_set_label_text(emote_handle_t handle, emote_obj_type_t obj_type,
                                 const char *text)
{
    if (!handle) {
        return false;
    }

    gfx_obj_t *obj = handle->gfx_objects[obj_type];
    if (!obj) {
        return false;
    }

    gfx_emote_lock(handle->gfx_emote_handle);
    gfx_label_set_text(obj, text ? text : "");
    gfx_obj_set_visible(obj, true);
    gfx_emote_unlock(handle->gfx_emote_handle);
    return true;
}

// Helper function to set emoji animation
static bool emote_set_emoji_animation(emote_handle_t handle, const char *emoji_name,
                                      emote_obj_type_t obj_type, void **cache_ptr, bool hide_eye)
{
    if (!handle || !emoji_name) {
        return false;
    }

    emoji_data_t *emoji = NULL;
    if (!emote_find_data_by_key(handle, handle->emoji_data, emoji_name, (void **)&emoji)) {
        return false;
    }

    ESP_LOGI(TAG, "Setting emoji: %s (fps=%d, loop=%s)",
             emoji_name, emoji->fps, emoji->loop ? "true" : "false");

    gfx_obj_t *obj = handle->gfx_objects[obj_type];
    if (!obj) {
        ESP_LOGE(TAG, "%s object not found", emoji_name);
        return false;
    }

    gfx_emote_lock(handle->gfx_emote_handle);
    const void *src_data = emote_acquire_data(handle, emoji->handle, emoji->data, emoji->size, cache_ptr);
    if (!src_data) {
        ESP_LOGE(TAG, "Failed to acquire %s animation data", emoji_name);
        gfx_emote_unlock(handle->gfx_emote_handle);
        return false;
    }

    gfx_anim_set_src(obj, src_data, emoji->size);
    gfx_anim_set_segment(obj, 0, 0xFFFF, emoji->fps > 0 ? emoji->fps : EMOTE_DEFAULT_ANIMATION_FPS, emoji->loop);
    gfx_anim_start(obj);
    gfx_obj_set_visible(obj, true);

    if (hide_eye) {
        gfx_obj_t *obj_eye = handle->gfx_objects[EMOTE_OBJ_ANIM_EYE];
        if (obj_eye) {
            gfx_obj_set_visible(obj_eye, false);
        }
    }
    gfx_emote_unlock(handle->gfx_emote_handle);
    return true;
}

// Timer callback for auto-stopping emergency dialog animation
static void emote_dialog_timer_cb(void *user_data)
{
    emote_handle_t handle = (emote_handle_t)user_data;
    if (!handle) {
        return;
    }

    // Delete timer
    if (handle->dialog_timer) {
        gfx_timer_delete(handle->gfx_emote_handle, handle->dialog_timer);
        handle->dialog_timer = NULL;
    }

    emote_stop_anim_dialog(handle);
}

// UI setting functions
static bool emote_set_image_tips(emote_handle_t handle, const char *name)
{
    return emote_set_icon_image(handle, name,
                                EMOTE_OBJ_ICON_STATUS, &handle->tips_icon_cache, &handle->tips_img_dsc, true);
}

static bool emote_set_image_charging(emote_handle_t handle, const char *name, bool is_charging)
{
    return emote_set_icon_image(handle, name,
                                EMOTE_OBJ_ICON_CHARGE, &handle->charge_icon_cache, &handle->charge_img_dsc, is_charging);
}

static bool emote_set_anim_listen(emote_handle_t handle, const char *name)
{
    return emote_set_icon_animation(handle, name, EMOTE_OBJ_ANIM_LISTEN,
                                    &handle->listen_anim_cache, 15, true);
}

static bool emote_set_label_toast(emote_handle_t handle, const char *text)
{
    return emote_set_label_text(handle, EMOTE_OBJ_LABEL_TOAST, text);
}

static bool emote_set_label_battery(emote_handle_t handle, const char *text)
{
    return emote_set_label_text(handle, EMOTE_OBJ_LABEL_BATTERY, text);
}

bool emote_set_bat_status(emote_handle_t handle)
{
    if (!handle) {
        return false;
    }

    if (handle->battery_percent >= 0) {
        char percent_str[16];
        snprintf(percent_str, sizeof(percent_str), "%d", handle->battery_percent);
        emote_set_image_tips(handle, EMOTE_ICON_BATTERY_BG);
        emote_set_label_battery(handle, percent_str);
        emote_set_image_charging(handle, EMOTE_ICON_BATTERY_CHARGE, handle->battery_is_charging);
    }
    return true;
}

bool emote_set_label_clock(emote_handle_t handle)
{
    if (!handle) {
        return false;
    }

    gfx_obj_t *obj = handle->gfx_objects[EMOTE_OBJ_LABEL_CLOCK];
    if (!obj) {
        ESP_LOGE(TAG, "CLOCK_LABEL object not found");
        return false;
    }

    gfx_timer_handle_t timer = handle->gfx_objects[EMOTE_OBJ_TIMER_STATUS];
    if (!timer) {
        ESP_LOGE(TAG, "CLOCK_TIMER object not found");
        return false;
    }

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    char time_str[10];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    gfx_emote_lock(handle->gfx_emote_handle);
    gfx_label_set_text(obj, time_str);
    gfx_obj_set_visible(obj, true);

    if (!gfx_timer_is_running(timer)) {
        gfx_timer_resume(timer);
    }
    gfx_emote_unlock(handle->gfx_emote_handle);
    return true;
}

bool emote_set_anim_emoji(emote_handle_t handle, const char *emoji_name)
{
    return emote_set_emoji_animation(handle, emoji_name, EMOTE_OBJ_ANIM_EYE,
                                     &handle->emoji_anim_cache, false);
}

bool emote_set_dialog_anim(emote_handle_t handle, const char *emoji_name)
{
    return emote_set_emoji_animation(handle, emoji_name, EMOTE_OBJ_ANIM_EMERG_DLG,
                                     &handle->emerg_dlg_cache, true);
}

bool emote_set_qrcode_data(emote_handle_t handle, const char *qrcode_text)
{
    ESP_LOGI(TAG, "set_qrcode_data: %s", qrcode_text);
    if (!handle || !qrcode_text) {
        return false;
    }
    gfx_obj_t *obj = handle->gfx_objects[EMOTE_OBJ_QRCODE];
    if (!obj) {
        ESP_LOGE(TAG, "QRCODE object not found");
        return false;
    }
    gfx_emote_lock(handle->gfx_emote_handle);
    gfx_qrcode_set_data(obj, qrcode_text);
    gfx_obj_set_visible(obj, true);
    gfx_emote_unlock(handle->gfx_emote_handle);
    return true;
}

bool emote_stop_anim_dialog(emote_handle_t handle)
{
    if (!handle) {
        return false;
    }

    gfx_emote_lock(handle->gfx_emote_handle);

    // Stop and delete timer if exists
    if (handle->dialog_timer) {
        gfx_timer_delete(handle->gfx_emote_handle, handle->dialog_timer);
        handle->dialog_timer = NULL;
    }

    gfx_obj_t *obj_emerg_dlg = handle->gfx_objects[EMOTE_OBJ_ANIM_EMERG_DLG];
    if (obj_emerg_dlg) {
        gfx_obj_set_visible(obj_emerg_dlg, false);
    }

    gfx_obj_t *obj_eye = handle->gfx_objects[EMOTE_OBJ_ANIM_EYE];
    if (obj_eye) {
        gfx_obj_set_visible(obj_eye, true);
    }
    gfx_emote_unlock(handle->gfx_emote_handle);

    if (handle->emerg_dlg_cache) {
        free(handle->emerg_dlg_cache);
        handle->emerg_dlg_cache = NULL;
    }
    return true;
}

bool emote_insert_anim_dialog(emote_handle_t handle, const char *emoji_name, uint32_t duration_ms)
{
    if (!handle || !emoji_name) {
        return false;
    }

    gfx_emote_lock(handle->gfx_emote_handle);
    if (handle->dialog_timer) {
        gfx_timer_delete(handle->gfx_emote_handle, handle->dialog_timer);
        handle->dialog_timer = NULL;
    }
    gfx_emote_unlock(handle->gfx_emote_handle);

    if (!emote_set_dialog_anim(handle, emoji_name)) {
        return false;
    }

    gfx_emote_lock(handle->gfx_emote_handle);

    gfx_timer_handle_t timer = gfx_timer_create(handle->gfx_emote_handle, emote_dialog_timer_cb, duration_ms, handle);
    if (!timer) {
        ESP_LOGE(TAG, "Failed to create dialog timer");
        gfx_emote_unlock(handle->gfx_emote_handle);
        emote_stop_anim_dialog(handle);
        return false;
    }

    gfx_timer_set_repeat_count(timer, 1);  // Execute only once
    handle->dialog_timer = timer;
    gfx_emote_unlock(handle->gfx_emote_handle);

    return true;
}

bool emote_set_event_msg(emote_handle_t handle, const char *event, const char *message)
{
    if (!handle || !event) {
        return false;
    }

    ESP_LOGD(TAG, "setEventMsg: %s, message: \"%s\"", event, message ? message : "");

    gfx_emote_lock(handle->gfx_emote_handle);

    // Look up event handler in table
    const emote_event_entry_t *entry = NULL;
    for (size_t i = 0; i < EVENT_TABLE_SIZE; i++) {
        if (strcmp(event, event_table[i].event_name) == 0) {
            entry = &event_table[i];
            break;
        }
    }

    if (!entry) {
        gfx_emote_unlock(handle->gfx_emote_handle);
        ESP_LOGE(TAG, "Unhandled event: %s", event);
        return false;
    }

    // Hide all UI elements for events that don't skip hiding
    if (!entry->skip_hide_ui) {
        HIDE_OBJ(handle, EMOTE_OBJ_ANIM_LISTEN);
        HIDE_OBJ(handle, EMOTE_OBJ_LABEL_CLOCK);
        HIDE_OBJ(handle, EMOTE_OBJ_LABEL_TOAST);
        HIDE_OBJ(handle, EMOTE_OBJ_LABEL_BATTERY);
        HIDE_OBJ(handle, EMOTE_OBJ_ICON_CHARGE);
        HIDE_OBJ(handle, EMOTE_OBJ_ICON_STATUS);
        HIDE_OBJ(handle, EMOTE_OBJ_QRCODE);
        gfx_obj_t *obj_timer = handle->gfx_objects[EMOTE_OBJ_TIMER_STATUS];
        if (obj_timer) {
            gfx_timer_pause((gfx_timer_handle_t)obj_timer);
        }
    }

    // Call event handler
    bool result = entry->handler(handle, message);

    gfx_emote_unlock(handle->gfx_emote_handle);

    return result;
}

bool emote_wait_boot_anim_stop(emote_handle_t handle, bool delete_anim)
{
    if (!handle) {
        return false;
    }

    ESP_LOGI(TAG, "Waiting for boot animation to stop");

    while (!handle->boot_anim_completed) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (delete_anim) {
        gfx_emote_lock(handle->gfx_emote_handle);
        if (handle->boot_anim_cache) {
            free(handle->boot_anim_cache);
            handle->boot_anim_cache = NULL;
        }

        if (handle->boot_assets_handle) {
            mmap_assets_del(handle->boot_assets_handle);
            handle->boot_assets_handle = NULL;
        }

        gfx_emote_set_bg_color(handle->gfx_emote_handle, GFX_COLOR_HEX(0x000000));
        gfx_obj_t *obj = handle->gfx_objects[EMOTE_OBJ_ANIM_BOOT];
        if (obj) {
            gfx_obj_delete(obj);
            handle->gfx_objects[EMOTE_OBJ_ANIM_BOOT] = NULL;
        }
        gfx_emote_unlock(handle->gfx_emote_handle);
    }

    return true;
}

bool emote_notify_flush_finished(emote_handle_t handle)
{
    if (!handle || !handle->is_initialized) {
        return false;
    }

    if (handle->gfx_emote_handle) {
        gfx_emote_flush_ready(handle->gfx_emote_handle, true);
        return true;
    }
    return false;
}
