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

#include "expression_emote.h"

#include "emote_load.h"
#include "emote_init.h"
#include "emote_setup.h"
#include "gfx.h"
#include "cJSON.h"

static const char *TAG = "ExpressionEmote";


// Hash table implementation
#define ASSETS_HASH_TABLE_SIZE 64

typedef struct assets_hash_entry_s {
    char *key;
    void *value;
    struct assets_hash_entry_s *next;
} assets_hash_entry_t;

struct assets_hash_table_s {
    assets_hash_entry_t *buckets[ASSETS_HASH_TABLE_SIZE];
};

static uint32_t emote_assets_hash_string(const char *str)
{
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

assets_hash_table_t *emote_assets_table_create(void)
{
    assets_hash_table_t *ht = (assets_hash_table_t *)calloc(1, sizeof(assets_hash_table_t));
    return ht;
}

void emote_assets_table_destroy(assets_hash_table_t *ht)
{
    if (!ht) {
        return;
    }

    for (int i = 0; i < ASSETS_HASH_TABLE_SIZE; i++) {
        assets_hash_entry_t *entry = ht->buckets[i];
        while (entry) {
            assets_hash_entry_t *next = entry->next;
            free(entry->key);
            // Free the value if it's a dynamically allocated structure
            if (entry->value) {
                free(entry->value);
            }
            free(entry);
            entry = next;
        }
    }
    free(ht);
}

static bool emote_assets_table_set(assets_hash_table_t *ht, const char *key, void *value)
{
    if (!ht || !key) {
        return false;
    }

    uint32_t hash = emote_assets_hash_string(key) % ASSETS_HASH_TABLE_SIZE;

    assets_hash_entry_t *entry = ht->buckets[hash];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return true;
        }
        entry = entry->next;
    }

    entry = (assets_hash_entry_t *)malloc(sizeof(assets_hash_entry_t));
    if (!entry) {
        return false;
    }
    entry->key = strdup(key);
    entry->value = value;
    entry->next = ht->buckets[hash];
    ht->buckets[hash] = entry;
    return true;
}

static void *emote_assets_table_get(assets_hash_table_t *ht, const char *key)
{
    if (!ht || !key) {
        return NULL;
    }

    uint32_t hash = emote_assets_hash_string(key) % ASSETS_HASH_TABLE_SIZE;

    assets_hash_entry_t *entry = ht->buckets[hash];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

const void *emote_acquire_data(emote_handle_t handle, mmap_assets_handle_t asset_handle,
                               const void *data_ref, size_t size, void **output_ptr)
{
    if (!handle) {
        return NULL;
    }

    const size_t OFFSET_THRESHOLD = 0x1000000;
    bool is_executed = ((size_t)data_ref < OFFSET_THRESHOLD);

    if (!is_executed || asset_handle == NULL) {
        if (output_ptr && *output_ptr) {
            free(*output_ptr);
            *output_ptr = NULL;
        }
        return data_ref;
    }

    if (output_ptr && *output_ptr) {
        free(*output_ptr);
        *output_ptr = NULL;
    }

    uint8_t *buffer = (uint8_t *)malloc(size);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory: %zu bytes", size);
        return NULL;
    }

    mmap_assets_copy_mem(asset_handle, (size_t)data_ref, buffer, size);

    if (output_ptr) {
        *output_ptr = buffer;
    }

    return buffer;
}

bool emote_find_data_by_name(mmap_assets_handle_t handle, const char *name,
                             const uint8_t **data, size_t *size)
{
    if (!name || !data || !size || !handle) {
        return false;
    }

    int fileNum = mmap_assets_get_stored_files(handle);
    for (int i = 0; i < fileNum; i++) {
        const char *currentFileName = mmap_assets_get_name(handle, i);
        if (currentFileName && strcmp(currentFileName, name) == 0) {
            const uint8_t *fileData = mmap_assets_get_mem(handle, i);
            size_t fileSize = mmap_assets_get_size(handle, i);
            if (fileData && fileSize > 0) {
                *data = fileData;
                *size = fileSize;
                return true;
            }
        }
    }

    ESP_LOGE(TAG, "Asset file not found: %s", name);
    return false;
}

bool emote_find_data_by_key(emote_handle_t handle, assets_hash_table_t *ht, const char *key, void **result)
{
    if (!handle || !ht || !key || !result) {
        return false;
    }
    *result = emote_assets_table_get(ht, key);
    if (!*result) {
        ESP_LOGE(TAG, "Data not found: %s", key);
        return false;
    }
    return true;
}

static bool emote_load_assets_handle(emote_handle_t handle, const emote_data_t *data,
                                     mmap_assets_handle_t *target_handle, const char *log_prefix)
{
    if (!handle || !data || !target_handle) {
        return false;
    }

    mmap_assets_config_t asset_config;
    memset(&asset_config, 0, sizeof(asset_config));

    if (data->type == EMOTE_SOURCE_PATH) {
        ESP_LOGI(TAG, "%s from file: path=%s", log_prefix, data->source.path);
        asset_config.partition_label = data->source.path;
        asset_config.flags.use_fs = true;
        asset_config.flags.full_check = true;
    } else if (data->type == EMOTE_SOURCE_PARTITION) {
        ESP_LOGI(TAG, "%s from partition: label=%s", log_prefix, data->source.partition_label);
        asset_config.partition_label = data->source.partition_label;
        asset_config.flags.mmap_enable = true;
        asset_config.flags.full_check = true;
    } else {
        ESP_LOGE(TAG, "Unknown source type");
        return false;
    }

    if (*target_handle) {
        ESP_LOGI(TAG, "Deleting existing assets handle: %p", *target_handle);
        mmap_assets_del(*target_handle);
        *target_handle = NULL;
    }

    esp_err_t err = mmap_assets_new(&asset_config, target_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create mmap assets: %s", esp_err_to_name(err));
        return false;
    }

    int num = mmap_assets_get_stored_files(*target_handle);
    if (num <= 0) {
        ESP_LOGE(TAG, "No files found in assets");
        mmap_assets_del(*target_handle);
        *target_handle = NULL;
        return false;
    }


    for (int i = 0; i < num; i++) {
        const char *name = mmap_assets_get_name(*target_handle, i);
        ESP_LOGI(TAG, "Found file: %d, %s", i, name);
    }

    return true;
}

static bool emote_load_emojis(emote_handle_t handle, mmap_assets_handle_t asset_handle, cJSON *root)
{
    if (!handle || !root) {
        return false;
    }

    cJSON *emojiCollection = cJSON_GetObjectItem(root, "emoji_collection");
    if (!cJSON_IsArray(emojiCollection)) {
        return true;
    }

    int emojiCount = cJSON_GetArraySize(emojiCollection);
    ESP_LOGI(TAG, "Found %d emoji items", emojiCount);

    for (int i = 0; i < emojiCount; i++) {
        cJSON *icon = cJSON_GetArrayItem(emojiCollection, i);
        if (!cJSON_IsObject(icon)) {
            continue;
        }

        cJSON *name = cJSON_GetObjectItem(icon, "name");
        cJSON *file = cJSON_GetObjectItem(icon, "file");
        if (!cJSON_IsString(name) || !cJSON_IsString(file)) {
            continue;
        }

        const uint8_t *emojiData = NULL;
        size_t emojiSize = 0;
        if (!emote_find_data_by_name(asset_handle, file->valuestring, &emojiData, &emojiSize)) {
            ESP_LOGE(TAG, "Failed to get emoji data for: %s", file->valuestring);
            continue;
        }

        bool loopValue = false;
        int fpsValue = 0;

        cJSON *eaf = cJSON_GetObjectItem(icon, "eaf");
        if (cJSON_IsObject(eaf)) {
            cJSON *loop = cJSON_GetObjectItem(eaf, "loop");
            cJSON *fps = cJSON_GetObjectItem(eaf, "fps");
            loopValue = loop ? cJSON_IsTrue(loop) : false;
            fpsValue = fps ? fps->valueint : 0;
        }

        emoji_data_t *emoji_data = (emoji_data_t *)malloc(sizeof(emoji_data_t));
        if (!emoji_data) {
            return false;
        }

        emoji_data->data = emojiData;
        emoji_data->size = emojiSize;
        emoji_data->fps = fpsValue;
        emoji_data->loop = loopValue;
        emoji_data->handle = asset_handle;

        emote_assets_table_set(handle->emoji_data, name->valuestring, emoji_data);
    }

    return true;
}

static bool emote_load_icons(emote_handle_t handle, mmap_assets_handle_t asset_handle, cJSON *root)
{
    if (!handle || !root) {
        return false;
    }

    cJSON *iconCollection = cJSON_GetObjectItem(root, "icon_collection");
    if (!cJSON_IsArray(iconCollection)) {
        return true;
    }

    int iconCount = cJSON_GetArraySize(iconCollection);
    ESP_LOGI(TAG, "Found %d icon items", iconCount);

    for (int i = 0; i < iconCount; i++) {
        cJSON *icon = cJSON_GetArrayItem(iconCollection, i);
        if (!cJSON_IsObject(icon)) {
            continue;
        }

        cJSON *name = cJSON_GetObjectItem(icon, "name");
        cJSON *file = cJSON_GetObjectItem(icon, "file");
        if (!cJSON_IsString(name) || !cJSON_IsString(file)) {
            continue;
        }

        const uint8_t *iconData = NULL;
        size_t iconSize = 0;
        if (!emote_find_data_by_name(asset_handle, file->valuestring, &iconData, &iconSize)) {
            ESP_LOGE(TAG, "Failed to get icon data for: %s", file->valuestring);
            continue;
        }

        // emote_save_icon_data(handle, asset_handle, name->valuestring, iconData, iconSize);

        icon_data_t *icon_data = (icon_data_t *)malloc(sizeof(icon_data_t));
        if (!icon_data) {
            return false;
        }

        icon_data->data = iconData;
        icon_data->size = iconSize;
        icon_data->handle = asset_handle;

        emote_assets_table_set(handle->icon_data, name->valuestring, icon_data);
    }

    return true;
}


static bool emote_load_layouts(emote_handle_t handle, cJSON *root)
{
    if (!handle || !root) {
        return false;
    }

    cJSON *layoutJson = cJSON_GetObjectItem(root, "layout");
    if (!cJSON_IsArray(layoutJson)) {
        return true;
    }

    int layoutCount = cJSON_GetArraySize(layoutJson);
    ESP_LOGI(TAG, "Found %d layout items", layoutCount);

    for (int i = 0; i < layoutCount; i++) {
        cJSON *layout = cJSON_GetArrayItem(layoutJson, i);
        if (!cJSON_IsObject(layout)) {
            continue;
        }

        cJSON *type = cJSON_GetObjectItem(layout, "type");
        cJSON *name = cJSON_GetObjectItem(layout, "name");
        if (!cJSON_IsString(type) || !cJSON_IsString(name)) {
            ESP_LOGE(TAG, "Invalid layout item %d: missing required fields", i);
            continue;
        }

        const char *typeStr = type->valuestring;
        const char *obj_name = name->valuestring;

        bool result = false;
        if (strcmp(typeStr, "anim") == 0) {
            result = emote_apply_anim_layout(handle, obj_name, layout);
        } else if (strcmp(typeStr, "image") == 0) {
            result = emote_apply_image_layout(handle, obj_name, layout);
        } else if (strcmp(typeStr, "label") == 0) {
            result = emote_apply_label_layout(handle, obj_name, layout);
        } else if (strcmp(typeStr, "timer") == 0) {
            result = emote_apply_timer_layout(handle, obj_name, layout);
        } else if (strcmp(typeStr, "qrcode") == 0) {
            result = emote_apply_qrcode_layout(handle, obj_name, layout);
        } else {
            ESP_LOGE(TAG, "Unknown type: %s", typeStr);
        }

        if (!result) {
            ESP_LOGE(TAG, "Failed to apply layout for %s", obj_name);
        }
    }

    return true;
}

static bool emote_load_fonts(emote_handle_t handle, mmap_assets_handle_t asset_handle, cJSON *root)
{
    if (!handle || !root) {
        return false;
    }

    cJSON *font = cJSON_GetObjectItem(root, "text_font");
    if (!cJSON_IsString(font)) {
        return true;
    }

    const char *fontsTextFile = font->valuestring;
    ESP_LOGI(TAG, "Found text font file: %s", fontsTextFile);

    const uint8_t *fontData = NULL;
    size_t fontSize = 0;
    if (!emote_find_data_by_name(asset_handle, fontsTextFile, &fontData, &fontSize)) {
        ESP_LOGE(TAG, "Font file not found: %s", fontsTextFile);
        return false;
    }

    const void *src_data = emote_acquire_data(handle, asset_handle, fontData, fontSize, &handle->font_cache);
    if (!src_data) {
        ESP_LOGE(TAG, "Failed to get font data");
        return false;
    }

    emote_apply_fonts(handle, (uint8_t *)src_data);

    return true;
}

static bool emote_load_assets_data(emote_handle_t handle, mmap_assets_handle_t asset_handle)
{
    if (!handle || !asset_handle) {
        return false;
    }

    const uint8_t *asset_data = NULL;
    size_t asset_size = 0;
    if (!emote_find_data_by_name(asset_handle, EMOTE_INDEX_JSON_FILENAME, &asset_data, &asset_size)) {
        ESP_LOGE(TAG, "Failed to find %s in assets", EMOTE_INDEX_JSON_FILENAME);
        return false;
    }

    ESP_LOGI(TAG, "Found %s, size: %zu", EMOTE_INDEX_JSON_FILENAME, asset_size);

    void *internal_buf = NULL;
    const void *src_data = emote_acquire_data(handle, asset_handle, asset_data, asset_size, &internal_buf);
    if (!src_data) {
        ESP_LOGE(TAG, "Failed to resolve asset data");
        return false;
    }

    cJSON *root = cJSON_ParseWithLength((const char *)src_data, asset_size);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse %s", EMOTE_INDEX_JSON_FILENAME);
        if (internal_buf) {
            free(internal_buf);
        }
        return false;
    }

    bool result = true;
    if (!emote_load_emojis(handle, asset_handle, root)) {
        result = false;
    }
    if (!emote_load_icons(handle, asset_handle, root)) {
        result = false;
    }
    if (!emote_load_layouts(handle, root)) {
        result = false;
    }
    if (!emote_load_fonts(handle, asset_handle, root)) {
        result = false;
    }

    cJSON_Delete(root);
    if (internal_buf) {
        free(internal_buf);
    }

    emote_delete_boot_anim(handle);

    return result;
}

static bool emote_load_boot_anim(emote_handle_t handle, mmap_assets_handle_t asset_handle)
{
    if (!handle || !asset_handle) {
        return false;
    }

    const uint8_t *anim_data = NULL;
    size_t anim_size = 0;

    int fileNum = mmap_assets_get_stored_files(asset_handle);
    if (fileNum > 0) {
        anim_data = mmap_assets_get_mem(asset_handle, 0);
        anim_size = mmap_assets_get_size(asset_handle, 0);
    }

    if (!anim_data || anim_size == 0) {
        ESP_LOGE(TAG, "Boot animation not available, anim_data: %p, anim_size: %zu", anim_data, anim_size);
        return false;
    }

    const void *src_data = emote_acquire_data(handle, asset_handle, anim_data, anim_size, &handle->boot_anim_cache);
    if (!src_data) {
        ESP_LOGE(TAG, "Failed to get boot animation data");
        gfx_emote_unlock(handle->gfx_emote_handle);
        return false;
    }

    return emote_setup_boot_anim(handle, (uint8_t *)src_data, anim_size);
}

bool emote_load_boot_anim_from_source(emote_handle_t handle, const emote_data_t *data)
{
    if (!handle || !data) {
        return false;
    }

    if (!emote_load_assets_handle(handle, data, &handle->boot_assets_handle, "Loading boot anim")) {
        return false;
    }

    return emote_load_boot_anim(handle, handle->boot_assets_handle);
}

bool emote_load_assets_from_source(emote_handle_t handle, const emote_data_t *data)
{
    if (!handle || !data) {
        return false;
    }

    if (!emote_load_assets_handle(handle, data, &handle->emote_assets_handle, "Loading assets")) {
        return false;
    }

    return emote_load_assets_data(handle, handle->emote_assets_handle);
}
