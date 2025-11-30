/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "expression_emote.h"
#include "emote_types.h"
#include "widget/gfx_font_lvgl.h"
#include "cJSON.h"
#include "emote_setup.h"
#include "emote_load.h"
#include "emote_init.h"
#include "emote_op.h"

static const char *TAG = "ExpressionEmote";

LV_FONT_DECLARE(font_maison_neue_book_12);
LV_FONT_DECLARE(font_maison_neue_book_26);

// ===== Object Creation Table Definitions =====
typedef gfx_obj_t *(*obj_creator_t)(gfx_handle_t gfx_handle, emote_handle_t handle);

// Object configurator function type
typedef void (*obj_configurator_t)(gfx_obj_t *obj);

// Object creation table entry
typedef struct {
    emote_obj_type_t type;
    obj_creator_t creator;
    obj_configurator_t configurator;
} obj_creation_entry_t;

// Helper functions
typedef struct {
    const char *name;
    int value;
} align_map_t;

typedef struct {
    const char *name;
    gfx_text_align_t value;
} text_align_map_t;

typedef struct {
    const char *name;
    gfx_label_long_mode_t value;
} long_mode_map_t;

// Forward declarations for object creators
static gfx_obj_t *emote_create_anim_obj(gfx_handle_t gfx_handle, emote_handle_t handle);
static gfx_obj_t *emote_create_img_obj(gfx_handle_t gfx_handle, emote_handle_t handle);
static gfx_obj_t *emote_create_qrcode_obj(gfx_handle_t gfx_handle, emote_handle_t handle);
static gfx_obj_t *emote_create_label_obj(gfx_handle_t gfx_handle, emote_handle_t handle);
static gfx_obj_t *emote_create_timer_obj(gfx_handle_t gfx_handle, emote_handle_t handle);

// Forward declarations for object configurators
static void emote_config_anim_obj(gfx_obj_t *obj);
static void emote_config_img_obj(gfx_obj_t *obj);
static void emote_config_qrcode_obj(gfx_obj_t *obj);
static void emote_config_label_toast_obj(gfx_obj_t *obj);
static void emote_config_label_clock_obj(gfx_obj_t *obj);
static void emote_config_label_battery_obj(gfx_obj_t *obj);

static int emote_convert_align_str(const char *str)
{
    if (!str) {
        return GFX_ALIGN_DEFAULT;
    }

    static const align_map_t align_map[] = {
        { "GFX_ALIGN_TOP_LEFT", GFX_ALIGN_TOP_LEFT },
        { "GFX_ALIGN_TOP_MID", GFX_ALIGN_TOP_MID },
        { "GFX_ALIGN_TOP_RIGHT", GFX_ALIGN_TOP_RIGHT },
        { "GFX_ALIGN_LEFT_MID", GFX_ALIGN_LEFT_MID },
        { "GFX_ALIGN_CENTER", GFX_ALIGN_CENTER },
        { "GFX_ALIGN_RIGHT_MID", GFX_ALIGN_RIGHT_MID },
        { "GFX_ALIGN_BOTTOM_LEFT", GFX_ALIGN_BOTTOM_LEFT },
        { "GFX_ALIGN_BOTTOM_MID", GFX_ALIGN_BOTTOM_MID },
        { "GFX_ALIGN_BOTTOM_RIGHT", GFX_ALIGN_BOTTOM_RIGHT },
        { "GFX_ALIGN_OUT_TOP_LEFT", GFX_ALIGN_OUT_TOP_LEFT },
        { "GFX_ALIGN_OUT_TOP_MID", GFX_ALIGN_OUT_TOP_MID },
        { "GFX_ALIGN_OUT_TOP_RIGHT", GFX_ALIGN_OUT_TOP_RIGHT },
        { "GFX_ALIGN_OUT_LEFT_TOP", GFX_ALIGN_OUT_LEFT_TOP },
        { "GFX_ALIGN_OUT_LEFT_MID", GFX_ALIGN_OUT_LEFT_MID },
        { "GFX_ALIGN_OUT_LEFT_BOTTOM", GFX_ALIGN_OUT_LEFT_BOTTOM },
        { "GFX_ALIGN_OUT_RIGHT_TOP", GFX_ALIGN_OUT_RIGHT_TOP },
        { "GFX_ALIGN_OUT_RIGHT_MID", GFX_ALIGN_OUT_RIGHT_MID },
        { "GFX_ALIGN_OUT_RIGHT_BOTTOM", GFX_ALIGN_OUT_RIGHT_BOTTOM },
        { "GFX_ALIGN_OUT_BOTTOM_LEFT", GFX_ALIGN_OUT_BOTTOM_LEFT },
        { "GFX_ALIGN_OUT_BOTTOM_MID", GFX_ALIGN_OUT_BOTTOM_MID },
        { "GFX_ALIGN_OUT_BOTTOM_RIGHT", GFX_ALIGN_OUT_BOTTOM_RIGHT },
    };

    for (size_t i = 0; i < sizeof(align_map) / sizeof(align_map[0]); i++) {
        if (strcmp(str, align_map[i].name) == 0) {
            return align_map[i].value;
        }
    }

    return GFX_ALIGN_DEFAULT;
}

static gfx_text_align_t emote_convert_text_align_str(const char *str)
{
    if (!str) {
        return GFX_TEXT_ALIGN_CENTER;
    }

    static const text_align_map_t text_align_map[] = {
        { "GFX_TEXT_ALIGN_AUTO", GFX_TEXT_ALIGN_AUTO },
        { "GFX_TEXT_ALIGN_LEFT", GFX_TEXT_ALIGN_LEFT },
        { "GFX_TEXT_ALIGN_CENTER", GFX_TEXT_ALIGN_CENTER },
        { "GFX_TEXT_ALIGN_RIGHT", GFX_TEXT_ALIGN_RIGHT },
    };

    for (size_t i = 0; i < sizeof(text_align_map) / sizeof(text_align_map[0]); i++) {
        if (strcmp(str, text_align_map[i].name) == 0) {
            return text_align_map[i].value;
        }
    }

    return GFX_TEXT_ALIGN_CENTER;
}

static gfx_label_long_mode_t emote_convert_long_mode_str(const char *str)
{
    if (!str) {
        return GFX_LABEL_LONG_CLIP;
    }

    static const long_mode_map_t long_mode_map[] = {
        { "GFX_LABEL_LONG_WRAP", GFX_LABEL_LONG_WRAP },
        { "GFX_LABEL_LONG_SCROLL", GFX_LABEL_LONG_SCROLL },
        { "GFX_LABEL_LONG_CLIP", GFX_LABEL_LONG_CLIP },
        { "GFX_LABEL_LONG_SNAP", GFX_LABEL_LONG_SCROLL_SNAP },
    };

    for (size_t i = 0; i < sizeof(long_mode_map) / sizeof(long_mode_map[0]); i++) {
        if (strcmp(str, long_mode_map[i].name) == 0) {
            return long_mode_map[i].value;
        }
    }

    return GFX_LABEL_LONG_CLIP;
}

static emote_obj_type_t emote_get_element_type(const char *name)
{
    if (!name) {
        return EMOTE_OBJ_MAX;
    }

    if (strcmp(name, EMOTE_ELEMENT_BOOT_ANIM) == 0) {
        return EMOTE_OBJ_ANIM_BOOT;
    }
    if (strcmp(name, EMOTE_ELEMENT_EYE_ANIM) == 0) {
        return EMOTE_OBJ_ANIM_EYE;
    }
    if (strcmp(name, EMOTE_ELEMENT_EMERG_DLG) == 0) {
        return EMOTE_OBJ_ANIM_EMERG_DLG;
    }
    if (strcmp(name, EMOTE_ELEMENT_TOAST_LABEL) == 0) {
        return EMOTE_OBJ_LABEL_TOAST;
    }
    if (strcmp(name, EMOTE_ELEMENT_CLOCK_LABEL) == 0) {
        return EMOTE_OBJ_LABEL_CLOCK;
    }
    if (strcmp(name, EMOTE_ELEMENT_LISTEN_ANIM) == 0) {
        return EMOTE_OBJ_ANIM_LISTEN;
    }
    if (strcmp(name, EMOTE_ELEMENT_STATUS_ICON) == 0) {
        return EMOTE_OBJ_ICON_STATUS;
    }
    if (strcmp(name, EMOTE_ELEMENT_CHARGE_ICON) == 0) {
        return EMOTE_OBJ_ICON_CHARGE;
    }
    if (strcmp(name, EMOTE_ELEMENT_BAT_LEFT_LABEL) == 0) {
        return EMOTE_OBJ_LABEL_BATTERY;
    }
    if (strcmp(name, EMOTE_ELEMENT_TIMER_STATUS) == 0) {
        return EMOTE_OBJ_TIMER_STATUS;
    }
    if (strcmp(name, EMOTE_ELEMENT_QRCODE) == 0) {
        return EMOTE_OBJ_QRCODE;
    }

    return EMOTE_OBJ_MAX;
}

static void emote_status_timer_callback(void *user_data)
{
    emote_handle_t handle = (emote_handle_t)user_data;
    if (handle) {
        emote_set_label_clock(handle);
        emote_set_bat_status(handle);
    }
}

// ===== Object Creator Implementations =====
static gfx_obj_t *emote_create_anim_obj(gfx_handle_t gfx_handle, emote_handle_t handle)
{
    (void)handle;
    return gfx_anim_create(gfx_handle);
}

static gfx_obj_t *emote_create_img_obj(gfx_handle_t gfx_handle, emote_handle_t handle)
{
    (void)handle;
    return gfx_img_create(gfx_handle);
}

static gfx_obj_t *emote_create_qrcode_obj(gfx_handle_t gfx_handle, emote_handle_t handle)
{
    (void)handle;
    // TODO: Implement gfx_qrcode_create if not available
    // For now, return NULL or use alternative implementation
    return NULL;  // Placeholder - implement when gfx_qrcode API is available
}

static gfx_obj_t *emote_create_label_obj(gfx_handle_t gfx_handle, emote_handle_t handle)
{
    (void)handle;
    return gfx_label_create(gfx_handle);
}

static gfx_obj_t *emote_create_timer_obj(gfx_handle_t gfx_handle, emote_handle_t handle)
{
    return (gfx_obj_t *)gfx_timer_create(gfx_handle, emote_status_timer_callback, 1000, handle);
}

// ===== Object Configurator Implementations =====
static void emote_config_anim_obj(gfx_obj_t *obj)
{
    if (obj) {
        gfx_obj_set_pos(obj, 0, 0);
    }
}

static void emote_config_img_obj(gfx_obj_t *obj)
{
    if (obj) {
        gfx_obj_set_visible(obj, false);
    }
}

static void emote_config_qrcode_obj(gfx_obj_t *obj)
{
    if (obj) {
        // TODO: Implement gfx_qrcode_set_size if available
        // gfx_qrcode_set_size(obj, 150);
        gfx_obj_set_visible(obj, false);
    }
}

static void emote_config_label_toast_obj(gfx_obj_t *obj)
{
    if (!obj) {
        return;
    }

    gfx_obj_align(obj, GFX_ALIGN_TOP_MID, 0, EMOTE_DEFAULT_LABEL_Y_OFFSET);
    gfx_obj_set_size(obj, EMOTE_DEFAULT_LABEL_WIDTH, EMOTE_DEFAULT_LABEL_HEIGHT);
    gfx_label_set_text(obj, "");
    gfx_label_set_color(obj, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text_align(obj, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_long_mode(obj, GFX_LABEL_LONG_SCROLL);
    gfx_label_set_scroll_speed(obj, EMOTE_DEFAULT_SCROLL_SPEED);
    gfx_label_set_scroll_loop(obj, true);
    gfx_label_set_font(obj, (void *)&font_maison_neue_book_26);
    gfx_obj_set_visible(obj, true);
}

static void emote_config_label_clock_obj(gfx_obj_t *obj)
{
    if (!obj) {
        return;
    }

    gfx_obj_align(obj, GFX_ALIGN_TOP_MID, 0, EMOTE_DEFAULT_LABEL_Y_OFFSET);
    gfx_obj_set_size(obj, EMOTE_DEFAULT_LABEL_WIDTH, EMOTE_DEFAULT_LABEL_HEIGHT);
    gfx_label_set_text(obj, "");
    gfx_label_set_color(obj, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text_align(obj, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_long_mode(obj, GFX_LABEL_LONG_SCROLL);
    gfx_label_set_scroll_speed(obj, EMOTE_DEFAULT_SCROLL_SPEED);
    gfx_label_set_scroll_loop(obj, true);
    gfx_label_set_font(obj, (void *)&font_maison_neue_book_26);
    gfx_obj_set_visible(obj, true);
}

static void emote_config_label_battery_obj(gfx_obj_t *obj)
{
    if (!obj) {
        return;
    }

    gfx_obj_align(obj, GFX_ALIGN_TOP_MID, 0, EMOTE_DEFAULT_LABEL_Y_OFFSET);
    gfx_obj_set_size(obj, EMOTE_DEFAULT_LABEL_WIDTH, EMOTE_DEFAULT_LABEL_HEIGHT);
    gfx_label_set_text(obj, "");
    gfx_label_set_color(obj, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text_align(obj, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_long_mode(obj, GFX_LABEL_LONG_SCROLL);
    gfx_label_set_scroll_speed(obj, EMOTE_DEFAULT_SCROLL_SPEED);
    gfx_label_set_scroll_loop(obj, true);
    gfx_label_set_font(obj, (void *)&font_maison_neue_book_12);
    gfx_obj_set_visible(obj, true);
}

// Object creation lookup table
static const obj_creation_entry_t obj_creation_table[] = {
    { EMOTE_OBJ_ANIM_BOOT,      emote_create_anim_obj,  emote_config_anim_obj          },
    { EMOTE_OBJ_ANIM_EYE,       emote_create_anim_obj,  emote_config_anim_obj          },
    { EMOTE_OBJ_ANIM_LISTEN,    emote_create_anim_obj,  emote_config_anim_obj          },
    { EMOTE_OBJ_ANIM_EMERG_DLG, emote_create_anim_obj,  emote_config_anim_obj          },
    { EMOTE_OBJ_ICON_STATUS,    emote_create_img_obj,   emote_config_img_obj           },
    { EMOTE_OBJ_ICON_CHARGE,    emote_create_img_obj,   emote_config_img_obj           },
    { EMOTE_OBJ_LABEL_TOAST,    emote_create_label_obj, emote_config_label_toast_obj   },
    { EMOTE_OBJ_LABEL_CLOCK,    emote_create_label_obj, emote_config_label_clock_obj  },
    { EMOTE_OBJ_LABEL_BATTERY,  emote_create_label_obj, emote_config_label_battery_obj },
    { EMOTE_OBJ_QRCODE,         emote_create_qrcode_obj, emote_config_qrcode_obj       },
    { EMOTE_OBJ_TIMER_STATUS,   emote_create_timer_obj, NULL                           },
};

#define OBJ_CREATION_TABLE_SIZE (sizeof(obj_creation_table) / sizeof(obj_creation_table[0]))

static gfx_obj_t *emote_create_object(emote_handle_t handle, emote_obj_type_t type)
{
    if (!handle) {
        return NULL;
    }

    gfx_obj_t *existing = handle->gfx_objects[type];
    if (existing) {
        return existing;
    }

    gfx_handle_t gfx_handle = handle->gfx_emote_handle;
    if (!gfx_handle) {
        return NULL;
    }

    gfx_emote_lock(gfx_handle);

    // Look up object creation entry in table
    const obj_creation_entry_t *entry = NULL;
    for (size_t i = 0; i < OBJ_CREATION_TABLE_SIZE; i++) {
        if (obj_creation_table[i].type == type) {
            entry = &obj_creation_table[i];
            break;
        }
    }

    gfx_obj_t *obj = NULL;
    if (entry && entry->creator) {
        obj = entry->creator(gfx_handle, handle);
        if (obj && entry->configurator) {
            entry->configurator(obj);
        }
    }

    gfx_emote_unlock(gfx_handle);

    if (obj) {
        handle->gfx_objects[type] = obj;
    }

    return obj;
}

static gfx_obj_t *emote_create_obj_by_name(emote_handle_t handle, const char *name)
{
    ESP_LOGD(TAG, "create object by name: %s", name);
    emote_obj_type_t type = emote_get_element_type(name);
    if (type == EMOTE_OBJ_MAX) {
        ESP_LOGE(TAG, "Unknown element: %s", name);
        return NULL;
    }

    gfx_obj_t *obj = handle->gfx_objects[type];
    if (!obj) {
        obj = emote_create_object(handle, type);
    }

    return obj;
}

bool emote_apply_anim_layout(emote_handle_t handle, const char *name, cJSON *layout)
{
    if (!handle || !name || !layout) {
        return false;
    }

    cJSON *align = cJSON_GetObjectItem(layout, "align");
    cJSON *x = cJSON_GetObjectItem(layout, "x");
    cJSON *y = cJSON_GetObjectItem(layout, "y");

    if (!cJSON_IsString(align) || !cJSON_IsNumber(x) || !cJSON_IsNumber(y)) {
        ESP_LOGE(TAG, "Anim %s: missing align/x/y fields", name);
        return false;
    }

    const char *align_str = align->valuestring;
    int xVal = x->valueint;
    int yVal = y->valueint;

    bool autoMirror = false;
    cJSON *animObj = cJSON_GetObjectItem(layout, "anim");
    if (cJSON_IsObject(animObj)) {
        cJSON *mirror = cJSON_GetObjectItem(animObj, "mirror");
        if (cJSON_IsString(mirror)) {
            const char *mirrorStr = mirror->valuestring;
            autoMirror = (strcmp(mirrorStr, "auto") == 0 || strcmp(mirrorStr, "true") == 0);
        }
    }

    gfx_obj_t *obj = emote_create_obj_by_name(handle, name);
    if (!obj) {
        ESP_LOGE(TAG, "Failed to create anim: %s", name);
        return false;
    }

    gfx_emote_lock(handle->gfx_emote_handle);
    gfx_obj_align(obj, emote_convert_align_str(align_str), xVal, yVal);
    if (autoMirror) {
        gfx_anim_set_auto_mirror(obj, true);
    }
    gfx_obj_set_visible(obj, false);
    gfx_emote_unlock(handle->gfx_emote_handle);

    if (strcmp(name, EMOTE_ELEMENT_EYE_ANIM) == 0) {
        gfx_obj_t *obj_qrcode = emote_create_obj_by_name(handle, EMOTE_ELEMENT_QRCODE);
        if (obj_qrcode) {
            gfx_emote_lock(handle->gfx_emote_handle);
            gfx_obj_align(obj_qrcode, GFX_ALIGN_CENTER, 0, 30);
            gfx_obj_set_visible(obj_qrcode, false);
            gfx_emote_unlock(handle->gfx_emote_handle);
        }
    }

    return true;
}

bool emote_apply_image_layout(emote_handle_t handle, const char *name, cJSON *layout)
{
    if (!handle || !name || !layout) {
        return false;
    }

    cJSON *align = cJSON_GetObjectItem(layout, "align");
    cJSON *x = cJSON_GetObjectItem(layout, "x");
    cJSON *y = cJSON_GetObjectItem(layout, "y");

    if (!cJSON_IsString(align) || !cJSON_IsNumber(x) || !cJSON_IsNumber(y)) {
        ESP_LOGE(TAG, "Image %s: missing align/x/y fields", name);
        return false;
    }

    gfx_obj_t *obj = emote_create_obj_by_name(handle, name);
    if (!obj) {
        ESP_LOGE(TAG, "Failed to create image: %s", name);
        return false;
    }

    gfx_emote_lock(handle->gfx_emote_handle);
    gfx_obj_align(obj, emote_convert_align_str(align->valuestring), x->valueint, y->valueint);
    gfx_obj_set_visible(obj, false);
    gfx_emote_unlock(handle->gfx_emote_handle);

    return true;
}

bool emote_apply_label_layout(emote_handle_t handle, const char *name, cJSON *layout)
{
    if (!handle || !name || !layout) {
        return false;
    }

    cJSON *align = cJSON_GetObjectItem(layout, "align");
    cJSON *x = cJSON_GetObjectItem(layout, "x");
    cJSON *y = cJSON_GetObjectItem(layout, "y");

    if (!cJSON_IsString(align) || !cJSON_IsNumber(x) || !cJSON_IsNumber(y)) {
        ESP_LOGE(TAG, "Label %s: missing align/x/y fields", name);
        return false;
    }

    cJSON *width = cJSON_GetObjectItem(layout, "width");
    cJSON *height = cJSON_GetObjectItem(layout, "height");
    int w = cJSON_IsNumber(width) ? width->valueint : 0;
    int h = cJSON_IsNumber(height) ? height->valueint : 0;

    uint32_t color = EMOTE_DEFAULT_FONT_COLOR;
    const char *textAlign = "center";
    const char *longModeType = "clip";
    bool longModeLoop = false;
    int longModeSpeed = EMOTE_DEFAULT_SCROLL_SPEED;
    int longModeSnapInterval = 1500;



    cJSON *labelObj = cJSON_GetObjectItem(layout, "label");
    if (cJSON_IsObject(labelObj)) {
        cJSON *colorJson = cJSON_GetObjectItem(labelObj, "color");
        if (cJSON_IsNumber(colorJson)) {
            color = colorJson->valueint;
        }

        cJSON *textAlignJson = cJSON_GetObjectItem(labelObj, "text_align");
        if (cJSON_IsString(textAlignJson)) {
            textAlign = textAlignJson->valuestring;
        }

        cJSON *longMode = cJSON_GetObjectItem(labelObj, "long_mode");
        if (cJSON_IsObject(longMode)) {
            cJSON *longType = cJSON_GetObjectItem(longMode, "type");
            if (cJSON_IsString(longType)) {
                longModeType = longType->valuestring;
            }

            cJSON *loop = cJSON_GetObjectItem(longMode, "loop");
            if (cJSON_IsBool(loop)) {
                longModeLoop = cJSON_IsTrue(loop);
            }

            cJSON *speed = cJSON_GetObjectItem(longMode, "speed");
            if (cJSON_IsNumber(speed)) {
                longModeSpeed = speed->valueint;
            }

            cJSON *snap_interval = cJSON_GetObjectItem(longMode, "snap_interval");
            if (cJSON_IsNumber(snap_interval)) {
                longModeSnapInterval = snap_interval->valueint;
            }
        }
    }

    gfx_obj_t *obj = emote_create_obj_by_name(handle, name);
    if (!obj) {
        ESP_LOGE(TAG, "Failed to create label: %s", name);
        return false;
    }

    gfx_emote_lock(handle->gfx_emote_handle);
    gfx_obj_align(obj, emote_convert_align_str(align->valuestring), x->valueint, y->valueint);

    if (w > 0 && h > 0) {
        gfx_obj_set_size(obj, w, h);
    }

    gfx_label_set_color(obj, GFX_COLOR_HEX(color));
    gfx_label_set_text_align(obj, emote_convert_text_align_str(textAlign));
    gfx_label_set_long_mode(obj, emote_convert_long_mode_str(longModeType));

    if (strcmp(longModeType, "GFX_LABEL_LONG_SCROLL") == 0) {
        gfx_label_set_scroll_speed(obj, longModeSpeed);
        gfx_label_set_scroll_loop(obj, longModeLoop);
    } else if (strcmp(longModeType, "GFX_LABEL_LONG_SNAP") == 0) {
        gfx_label_set_snap_loop(obj, longModeLoop);
        gfx_label_set_snap_interval(obj, longModeSnapInterval);
    }

    gfx_obj_set_visible(obj, false);
    gfx_emote_unlock(handle->gfx_emote_handle);

    return true;
}

bool emote_apply_timer_layout(emote_handle_t handle, const char *name, cJSON *layout)
{
    if (!handle || !name || !layout) {
        return false;
    }

    uint32_t period = 1000;
    int32_t repeat_count = -1;

    cJSON *timerObj = cJSON_GetObjectItem(layout, "timer");
    if (!cJSON_IsObject(timerObj)) {
        ESP_LOGE(TAG, "Timer object not found for %s", name);
        return false;
    }

    cJSON *periodJson = cJSON_GetObjectItem(timerObj, "period");
    if (cJSON_IsNumber(periodJson)) {
        period = periodJson->valueint;
    }

    cJSON *repeatCountJson = cJSON_GetObjectItem(timerObj, "repeat_count");
    if (cJSON_IsNumber(repeatCountJson)) {
        repeat_count = repeatCountJson->valueint;
    }

    gfx_obj_t *obj = emote_create_obj_by_name(handle, name);
    if (!obj) {
        ESP_LOGE(TAG, "Failed to create timer: %s", name);
        return false;
    }

    gfx_emote_lock(handle->gfx_emote_handle);
    gfx_timer_set_repeat_count(obj, repeat_count);
    gfx_timer_set_period(obj, period);
    gfx_timer_pause((gfx_timer_handle_t)obj);
    gfx_emote_unlock(handle->gfx_emote_handle);

    return true;
}

bool emote_apply_fonts(emote_handle_t handle, const uint8_t *fontData)
{
    if (!handle || !fontData) {
        return false;
    }

    handle->gfx_font = gfx_font_lv_load_from_binary((uint8_t *)fontData);
    if (!handle->gfx_font) {
        ESP_LOGE(TAG, "Failed to create font");
        return false;
    }

    gfx_obj_t *obj = handle->gfx_objects[EMOTE_OBJ_LABEL_TOAST];
    if (obj) {
        gfx_emote_lock(handle->gfx_emote_handle);
        gfx_label_set_font(obj, handle->gfx_font);
        gfx_emote_unlock(handle->gfx_emote_handle);
    }

    return true;
}

void emote_delete_boot_anim(emote_handle_t handle)
{
    if (!handle) {
        return;
    }

    // Delete boot animation if exists
    gfx_obj_t *boot_obj = handle->gfx_objects[EMOTE_OBJ_ANIM_BOOT];
    if (boot_obj) {
        gfx_emote_lock(handle->gfx_emote_handle);
        gfx_obj_delete(boot_obj);
        handle->gfx_objects[EMOTE_OBJ_ANIM_BOOT] = NULL;
        gfx_emote_unlock(handle->gfx_emote_handle);
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    emote_set_event_msg(handle, EMOTE_MGR_EVT_SYS, "启动中...");
}

bool emote_setup_boot_anim(emote_handle_t handle, uint8_t *anim_data, size_t anim_size)
{
    if (!handle || !anim_data || (0 == anim_size)) {
        return false;
    }

    gfx_obj_t *obj = emote_create_obj_by_name(handle, EMOTE_ELEMENT_BOOT_ANIM);
    if (!obj) {
        ESP_LOGE(TAG, "Failed to create boot animation object");
        return false;
    }

    gfx_emote_lock(handle->gfx_emote_handle);
    gfx_emote_set_bg_color(handle->gfx_emote_handle, GFX_COLOR_HEX(0x000000));
    gfx_obj_set_visible(obj, true);
    gfx_obj_align(obj, GFX_ALIGN_CENTER, 0, 0);
    gfx_anim_set_src(obj, anim_data, anim_size);
    gfx_anim_set_segment(obj, 0, 0xFFFF, EMOTE_DEFAULT_ANIMATION_FPS, false);
    gfx_anim_start(obj);
    gfx_emote_unlock(handle->gfx_emote_handle);

    handle->boot_anim_completed = false;
    return true;
}
