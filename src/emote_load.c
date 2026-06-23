/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_CORE
#include "common/gfx_check.h"
#include "common/gfx_log_priv.h"
#include <string.h>
#include <stdlib.h>
#if GFX_HOST_BUILD
#include <sys/stat.h>
#endif

#include "expression_emote.h"

#include "emote_defs.h"
#include "emote_table.h"
#include "emote_layout.h"
#include "gfx.h"
#include "cJSON.h"

static const char *TAG = "Expression_load";

// Hash table implementation
#define ASSETS_HASH_TABLE_SIZE CONFIG_EMOTE_ASSETS_HASH_TABLE_SIZE

typedef struct assets_hash_entry_s {
    char *key;
    void *value;
    struct assets_hash_entry_s *next;
} assets_hash_entry_t;

struct assets_hash_table_s {
    char *name;
    assets_hash_entry_t *buckets[ASSETS_HASH_TABLE_SIZE];
};

struct emote_asset_blob_entry_s {
    char *name;
    const uint8_t *data;
    size_t size;
    bool owned;
    struct emote_asset_blob_entry_s *next;
};

#define EMOTE_ASSETS_OFFSET_THRESHOLD  0x1000000U

static bool emote_data_ref_is_pack_offset(const void *data_ref)
{
    return data_ref != NULL && ((uintptr_t)data_ref < EMOTE_ASSETS_OFFSET_THRESHOLD);
}

static emote_assets_addr_mode_t emote_query_assets_addr_mode(emote_handle_t handle)
{
    if (handle == NULL || handle->assets_fs == NULL) {
        return EMOTE_ASSETS_ADDR_COPY;
    }

    return gfx_fs_get_access_mode(handle->assets_fs) == GFX_FS_ACCESS_DIRECT ?
           EMOTE_ASSETS_ADDR_DIRECT :
           EMOTE_ASSETS_ADDR_COPY;
}

static bool emote_acquire_needs_cache_copy(emote_handle_t handle, const void *data_ref)
{
    if (handle == NULL || data_ref == NULL) {
        return false;
    }

    if (emote_query_assets_addr_mode(handle) == EMOTE_ASSETS_ADDR_COPY) {
        return false;
    }

    /* DIRECT mmap: borrow flash pointers; legacy pack offsets still need a copy. */
    return emote_data_ref_is_pack_offset(data_ref);
}

#if GFX_HOST_BUILD
static gfx_fs_source_type_t emote_host_path_source_type(const char *path)
{
    struct stat st;

    if (path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        return GFX_FS_SOURCE_PACK_FILE;
    }

    return GFX_FS_SOURCE_DIR;
}
#endif

static void emote_clear_asset_blobs(emote_handle_t handle)
{
    emote_asset_blob_entry_t *entry;

    if (handle == NULL) {
        return;
    }

    entry = handle->asset_blobs;
    while (entry != NULL) {
        emote_asset_blob_entry_t *next = entry->next;
        free(entry->name);
        if (entry->owned) {
            free((void *)entry->data);
        }
        free(entry);
        entry = next;
    }
    handle->asset_blobs = NULL;
}

static uint32_t emote_assets_hash_string(const char *str)
{
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

assets_hash_table_t *emote_assets_table_create(const char *name)
{
    assets_hash_table_t *ht = (assets_hash_table_t *)calloc(1, sizeof(assets_hash_table_t));
    if (ht && name) {
        ht->name = strdup(name);
        if (!ht->name) {
            free(ht);
            return NULL;
        }
    }
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
    if (ht->name) {
        free(ht->name);
    }
    free(ht);
}

static gfx_err_t emote_assets_table_set(assets_hash_table_t *ht, const char *key, void *value)
{
    gfx_err_t ret = GFX_OK;
    assets_hash_entry_t *entry = NULL;

    GFX_GOTO_ON_FALSE(ht && key, GFX_ERR_INVALID_ARG, error, TAG, "Invalid parameters");

    uint32_t hash = emote_assets_hash_string(key) % ASSETS_HASH_TABLE_SIZE;

    entry = ht->buckets[hash];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return GFX_OK;
        }
        entry = entry->next;
    }

    entry = (assets_hash_entry_t *)malloc(sizeof(assets_hash_entry_t));
    GFX_GOTO_ON_FALSE(entry, GFX_ERR_NO_MEM, error, TAG, "Failed to allocate hash entry");

    entry->key = strdup(key);
    GFX_GOTO_ON_FALSE(entry->key, GFX_ERR_NO_MEM, error_free_entry, TAG, "Failed to duplicate key");

    entry->value = value;
    entry->next = ht->buckets[hash];
    ht->buckets[hash] = entry;
    return GFX_OK;

error_free_entry:
    free(entry);

error:
    return ret;
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

const void *emote_acquire_data(emote_handle_t handle, const void *data_ref, size_t size, void **output_ptr)
{
    uint8_t *buffer;

    if (!handle) {
        return NULL;
    }

    if (output_ptr && *output_ptr) {
        free(*output_ptr);
        *output_ptr = NULL;
    }

    if (output_ptr == NULL || data_ref == NULL || size == 0U) {
        return data_ref;
    }

    if (!emote_acquire_needs_cache_copy(handle, data_ref)) {
        return data_ref;
    }

    buffer = (uint8_t *)malloc(size);
    if (buffer == NULL) {
        GFX_LOGE(TAG, "Failed to allocate %zu bytes", size);
        return NULL;
    }

    if (emote_data_ref_is_pack_offset(data_ref)) {
        GFX_LOGE(TAG, "Invalid pack offset reference %p", data_ref);
        free(buffer);
        return NULL;
    }

    memcpy(buffer, data_ref, size);
    *output_ptr = buffer;
    return buffer;
}

gfx_err_t emote_get_asset_data_by_name(emote_handle_t handle, const char *name,
                                       const uint8_t **data, size_t *size)
{
    gfx_err_t ret = GFX_OK;
    gfx_fs_file_t *file = NULL;
    const void *file_data;
    size_t file_size;
    uint8_t *copy = NULL;
    emote_asset_blob_entry_t *entry;
    emote_asset_blob_entry_t *new_entry = NULL;

    GFX_GOTO_ON_FALSE(name && data && size && handle && handle->assets_fs,
                      GFX_ERR_INVALID_ARG, error, TAG, "Invalid parameters");

    for (entry = handle->asset_blobs; entry != NULL; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            *data = entry->data;
            *size = entry->size;
            return GFX_OK;
        }
    }

    file = gfx_fs_fopen_from(handle->assets_fs, name);
    GFX_GOTO_ON_FALSE(file != NULL, GFX_ERR_NOT_FOUND, error, TAG, "Asset file not found: %s", name);

    file_size = gfx_fs_fsize(file);
    file_data = gfx_fs_fdata(file);
    GFX_GOTO_ON_FALSE(file_size > 0U, GFX_ERR_INVALID_SIZE, error, TAG, "Asset file is empty: %s", name);

    new_entry = (emote_asset_blob_entry_t *)calloc(1, sizeof(*new_entry));
    GFX_GOTO_ON_FALSE(new_entry != NULL, GFX_ERR_NO_MEM, error, TAG, "Failed to allocate asset entry");
    new_entry->name = strdup(name);
    GFX_GOTO_ON_FALSE(new_entry->name != NULL, GFX_ERR_NO_MEM, error, TAG, "Failed to duplicate asset name");
    new_entry->size = file_size;

    if (emote_query_assets_addr_mode(handle) == EMOTE_ASSETS_ADDR_DIRECT && file_data != NULL) {
        new_entry->data = (const uint8_t *)file_data;
        new_entry->owned = false;
    } else {
        copy = (uint8_t *)malloc(file_size);
        GFX_GOTO_ON_FALSE(copy != NULL, GFX_ERR_NO_MEM, error, TAG, "Failed to allocate asset cache: %s", name);
        if (file_data != NULL) {
            memcpy(copy, file_data, file_size);
        } else {
            GFX_GOTO_ON_FALSE(gfx_fs_fread(file, copy, file_size) == file_size,
                              GFX_FAIL, error, TAG, "Failed to read asset file: %s", name);
        }
        new_entry->data = copy;
        new_entry->owned = true;
        copy = NULL;
    }

    new_entry->next = handle->asset_blobs;
    handle->asset_blobs = new_entry;

    gfx_fs_fclose(file);
    *data = new_entry->data;
    *size = new_entry->size;
    return GFX_OK;

error:
    if (new_entry != NULL) {
        free(new_entry->name);
        free(new_entry);
    }
    free(copy);
    if (file != NULL) {
        gfx_fs_fclose(file);
    }
    return ret;
}

static gfx_err_t emote_find_data_by_key(emote_handle_t handle, assets_hash_table_t *ht, const char *key, void **result)
{
    if (!handle || !ht || !key || !result) {
        return GFX_ERR_INVALID_ARG;
    }

    *result = emote_assets_table_get(ht, key);
    return *result ? GFX_OK : GFX_ERR_NOT_FOUND;
}

gfx_err_t emote_unmount_assets(emote_handle_t handle)
{
    if (!handle) {
        return GFX_ERR_INVALID_ARG;
    }

    emote_clear_asset_blobs(handle);

    if (handle->assets_fs) {
        GFX_LOGI(TAG, "Unmounting assets fs");
        if (gfx_fs_get_default() == handle->assets_fs) {
            gfx_fs_set_default(NULL);
        }
        gfx_fs_close(handle->assets_fs);
        handle->assets_fs = NULL;
    }

    return GFX_OK;
}

gfx_err_t emote_mount_assets(emote_handle_t handle, const emote_data_t *data)
{
    gfx_err_t ret = GFX_OK;
    gfx_err_t gfx_err;

    GFX_GOTO_ON_FALSE(handle && data, GFX_ERR_INVALID_ARG, error, TAG, "Invalid parameters");

    // Unmount existing assets first
    ret = emote_unmount_assets(handle);
    GFX_GOTO_ON_FALSE(ret == GFX_OK, ret, error, TAG, "Failed to unmount existing assets");

    if (data->type == EMOTE_SOURCE_PATH) {
#if GFX_HOST_BUILD
        const gfx_fs_source_type_t source_type = emote_host_path_source_type(data->source.path);
#else
        const gfx_fs_source_type_t source_type = GFX_FS_SOURCE_PACK_FILE;
#endif

        GFX_LOGI(TAG, "Loading assets from file: path=%s", data->source.path);
        gfx_err = gfx_fs_open(&(gfx_fs_open_config_t) {
            .source_type = source_type,
            .access_mode = GFX_FS_ACCESS_COPY,
            .path_or_label = data->source.path,
        }, &handle->assets_fs);
    } else if (data->type == EMOTE_SOURCE_PARTITION) {
        const gfx_fs_access_mode_t access_mode = data->flags.mmap_enable ?
                                                 GFX_FS_ACCESS_DIRECT :
                                                 GFX_FS_ACCESS_COPY;

        GFX_LOGI(TAG, "Loading assets from partition: label=%s, access=%s",
                 data->source.partition_label,
                 access_mode == GFX_FS_ACCESS_DIRECT ? "direct" : "copy");
        gfx_err = gfx_fs_open(&(gfx_fs_open_config_t) {
            .source_type = GFX_FS_SOURCE_PARTITION,
            .access_mode = access_mode,
            .path_or_label = data->source.partition_label,
        }, &handle->assets_fs);
    } else {
        ret = GFX_ERR_INVALID_ARG;
        GFX_LOGE(TAG, "Unknown source type");
        goto error;
    }

    ret = gfx_err;
    GFX_GOTO_ON_FALSE(ret == GFX_OK, ret, error, TAG, "Failed to open assets fs: %d", (int)ret);
    gfx_fs_set_default(handle->assets_fs);
    GFX_LOGI(TAG, "Assets addr mode: %s",
             emote_query_assets_addr_mode(handle) == EMOTE_ASSETS_ADDR_DIRECT ? "direct" : "copy");

    return GFX_OK;

error:
    return ret;
}

static gfx_err_t emote_load_emojis(emote_handle_t handle, cJSON *root)
{
    gfx_err_t ret = GFX_OK;
    cJSON *emojiCollection = NULL;
    int emojiCount = 0;

    GFX_GOTO_ON_FALSE(handle && root, GFX_ERR_INVALID_ARG, error, TAG, "Invalid parameters");

    emojiCollection = cJSON_GetObjectItem(root, "emoji_collection");
    if (!cJSON_IsArray(emojiCollection)) {
        return GFX_OK;
    }

    emojiCount = cJSON_GetArraySize(emojiCollection);
    GFX_LOGI(TAG, "Found %d emoji items", emojiCount);

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
        ret = emote_get_asset_data_by_name(handle, file->valuestring, &emojiData, &emojiSize);
        if (ret != GFX_OK) {
            GFX_LOGE(TAG, "Failed to get emoji data for: %s", file->valuestring);
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
        GFX_GOTO_ON_FALSE(emoji_data, GFX_ERR_NO_MEM, error, TAG, "Failed to allocate emoji data");

        emoji_data->data = emojiData;
        emoji_data->size = emojiSize;
        emoji_data->fps = fpsValue;
        emoji_data->loop = loopValue;

        GFX_LOGD(TAG, "set emoji data: %s", name->valuestring);
        ret = emote_assets_table_set(handle->emoji_table, name->valuestring, emoji_data);
        if (ret != GFX_OK) {
            GFX_LOGW(TAG, "Failed to set emoji data for: %s", name->valuestring);
            free(emoji_data);
        }
    }

    return GFX_OK;

error:
    return ret;
}

static gfx_err_t emote_load_icons(emote_handle_t handle, cJSON *root)
{
    gfx_err_t ret = GFX_OK;
    cJSON *iconCollection = NULL;
    int iconCount = 0;

    GFX_GOTO_ON_FALSE(handle && root, GFX_ERR_INVALID_ARG, error, TAG, "Invalid parameters");

    iconCollection = cJSON_GetObjectItem(root, "icon_collection");
    if (!cJSON_IsArray(iconCollection)) {
        return GFX_OK;
    }

    iconCount = cJSON_GetArraySize(iconCollection);
    GFX_LOGI(TAG, "Found %d icon items", iconCount);

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
        ret = emote_get_asset_data_by_name(handle, file->valuestring, &iconData, &iconSize);
        if (ret != GFX_OK) {
            GFX_LOGE(TAG, "Failed to get icon data for: %s", file->valuestring);
            continue;
        }

        icon_data_t *icon_data = (icon_data_t *)malloc(sizeof(icon_data_t));
        GFX_GOTO_ON_FALSE(icon_data, GFX_ERR_NO_MEM, error, TAG, "Failed to allocate icon data");

        icon_data->data = iconData;
        icon_data->size = iconSize;

        GFX_LOGD(TAG, "set icon data: %s", name->valuestring);
        ret = emote_assets_table_set(handle->icon_table, name->valuestring, icon_data);
        if (ret != GFX_OK) {
            GFX_LOGW(TAG, "Failed to set icon data for: %s", name->valuestring);
            free(icon_data);
        }
    }

    return GFX_OK;

error:
    return ret;
}

static gfx_err_t emote_load_layouts(emote_handle_t handle, cJSON *root)
{
    gfx_err_t ret = GFX_OK;
    cJSON *layoutJson = NULL;
    int layoutCount = 0;

    GFX_GOTO_ON_FALSE(handle && root, GFX_ERR_INVALID_ARG, error, TAG, "Invalid parameters");

    layoutJson = cJSON_GetObjectItem(root, "layout");
    if (!cJSON_IsArray(layoutJson)) {
        return GFX_OK;
    }

    layoutCount = cJSON_GetArraySize(layoutJson);
    GFX_LOGI(TAG, "Found %d layout items", layoutCount);

    for (int i = 0; i < layoutCount; i++) {
        cJSON *layout = cJSON_GetArrayItem(layoutJson, i);
        if (!cJSON_IsObject(layout)) {
            continue;
        }

        cJSON *type = cJSON_GetObjectItem(layout, "type");
        cJSON *name = cJSON_GetObjectItem(layout, "name");
        if (!cJSON_IsString(type) || !cJSON_IsString(name)) {
            GFX_LOGE(TAG, "Invalid layout item %d: missing required fields", i);
            continue;
        }

        const char *typeStr = type->valuestring;
        const char *obj_name = name->valuestring;

        ret = GFX_ERR_INVALID_ARG;
        if (strcmp(typeStr, "anim") == 0) {
            ret = emote_apply_anim_layout(handle, obj_name, layout);
        } else if (strcmp(typeStr, "image") == 0) {
            ret = emote_apply_image_layout(handle, obj_name, layout);
        } else if (strcmp(typeStr, "label") == 0) {
            ret = emote_apply_label_layout(handle, obj_name, layout);
        } else if (strcmp(typeStr, "timer") == 0) {
            ret = emote_apply_timer_layout(handle, obj_name, layout);
        } else if (strcmp(typeStr, "qrcode") == 0) {
            ret = emote_apply_qrcode_layout(handle, obj_name, layout);
        } else {
            GFX_LOGE(TAG, "Unknown type: %s", typeStr);
        }

        if (ret != GFX_OK) {
            GFX_LOGE(TAG, "Failed to apply layout for %s: %s", obj_name, (int)ret);
        }
    }

    if (ret == GFX_OK) {
        gfx_object_t *obj_default = handle->def_objects[EMOTE_DEF_OBJ_LEBAL_DEFAULT].obj;
        if (obj_default) {
            gfx_object_delete(obj_default);
            handle->def_objects[EMOTE_DEF_OBJ_LEBAL_DEFAULT].obj = NULL;
        }
    }

    return GFX_OK;

error:
    return ret;
}

static gfx_err_t emote_load_fonts(emote_handle_t handle, cJSON *root)
{
    gfx_err_t ret = GFX_OK;
    cJSON *font = NULL;
    const uint8_t *fontData = NULL;
    size_t fontSize = 0;
    const void *src_data = NULL;

    GFX_GOTO_ON_FALSE(handle && root, GFX_ERR_INVALID_ARG, error, TAG, "Invalid parameters");

    font = cJSON_GetObjectItem(root, "text_font");
    if (!cJSON_IsString(font)) {
        return GFX_OK;
    }

    const char *fontsTextFile = font->valuestring;
    GFX_LOGI(TAG, "Foundfont: %s", fontsTextFile);

    ret = emote_get_asset_data_by_name(handle, fontsTextFile, &fontData, &fontSize);
    GFX_GOTO_ON_FALSE(ret == GFX_OK, ret, error, TAG, "Font file not found: %s", fontsTextFile);

    src_data = emote_acquire_data(handle, fontData, fontSize, &handle->font_cache);
    GFX_GOTO_ON_FALSE(src_data, GFX_ERR_INVALID_STATE, error, TAG, "Failed to get font data");

    ret = emote_apply_fonts(handle, (uint8_t *)src_data);
    GFX_GOTO_ON_FALSE(ret == GFX_OK, ret, error, TAG, "Failed to apply fonts: %d", (int)ret);

    return GFX_OK;

error:
    return ret;
}

gfx_err_t emote_load_assets(emote_handle_t handle)
{
    gfx_err_t ret = GFX_OK;
    const uint8_t *asset_data = NULL;
    size_t asset_size = 0;
    void *internal_buf = NULL;
    const void *src_data = NULL;
    cJSON *root = NULL;

    GFX_GOTO_ON_FALSE(handle, GFX_ERR_INVALID_ARG, error, TAG, "Invalid parameters");

    // Create hash tables if they don't exist
    if (!handle->emoji_table) {
        handle->emoji_table = emote_assets_table_create("emoji");
        GFX_GOTO_ON_FALSE(handle->emoji_table, GFX_ERR_NO_MEM, error, TAG, "Failed to create emoji_table hash table");
    }

    if (!handle->icon_table) {
        handle->icon_table = emote_assets_table_create("icon");
        GFX_GOTO_ON_FALSE(handle->icon_table, GFX_ERR_NO_MEM, error, TAG, "Failed to create icon_table hash table");
    }

    // Create semaphore for emergency dialog animation completion
    if (!handle->emerg_dlg_done_event) {
        handle->emerg_dlg_done_event = gfx_platform_event_create();
        GFX_GOTO_ON_FALSE(handle->emerg_dlg_done_event, GFX_ERR_NO_MEM, error, TAG,
                          "Failed to create emerg_dlg_done_event");
    }

    ret = emote_get_asset_data_by_name(handle, EMOTE_INDEX_JSON_FILENAME, &asset_data, &asset_size);
    GFX_GOTO_ON_FALSE(ret == GFX_OK, ret, error, TAG, "Failed to find %s in assets", EMOTE_INDEX_JSON_FILENAME);

    GFX_LOGI(TAG, "Found %s, size: %d", EMOTE_INDEX_JSON_FILENAME, (int)asset_size);

    src_data = emote_acquire_data(handle, asset_data, asset_size, &internal_buf);
    GFX_GOTO_ON_FALSE(src_data, GFX_ERR_INVALID_STATE, error, TAG, "Failed to resolve asset data");

    root = cJSON_ParseWithLength((const char *)src_data, asset_size);
    GFX_GOTO_ON_FALSE(root, GFX_ERR_INVALID_RESPONSE, error_free_buf, TAG, "Failed to parse %s", EMOTE_INDEX_JSON_FILENAME);

    ret = emote_load_emojis(handle, root);
    if (ret != GFX_OK) {
        GFX_LOGW(TAG, "Failed to load emojis: %d", (int)ret);
    }

    ret = emote_load_icons(handle, root);
    if (ret != GFX_OK) {
        GFX_LOGW(TAG, "Failed to load icons: %d", (int)ret);
    }

    ret = emote_load_layouts(handle, root);
    if (ret != GFX_OK) {
        GFX_LOGW(TAG, "Failed to load layouts: %d", (int)ret);
    }

    ret = emote_load_fonts(handle, root);
    if (ret != GFX_OK) {
        GFX_LOGW(TAG, "Failed to load fonts: %d", (int)ret);
    }

    cJSON_Delete(root);
    if (internal_buf) {
        free(internal_buf);
    }

    return GFX_OK;

error_free_buf:
    if (internal_buf) {
        free(internal_buf);
    }

error:
    return ret;
}

gfx_err_t emote_unload_assets(emote_handle_t handle)
{
    gfx_err_t ret = GFX_OK;

    GFX_GOTO_ON_FALSE(handle, GFX_ERR_INVALID_ARG, error, TAG, "Invalid parameters");

    // Cleanup objects
    if (handle->gfx_handle) {
        gfx_core_lock(handle->gfx_handle);
        // Cleanup def_objects
        for (int i = EMOTE_DEF_OBJ_ANIM_EYE; i < EMOTE_DEF_OBJ_MAX; i++) {
            emote_def_obj_entry_t *entry = &handle->def_objects[i];
            if (entry->obj) {
                if (i == EMOTE_DEF_OBJ_TIMER_STATUS) {
                    gfx_timer_delete(handle->gfx_handle, (gfx_timer_handle_t)entry->obj);
                } else {
                    gfx_object_delete(entry->obj);
                }
                entry->obj = NULL;
            }
            // Cleanup cache based on object type
            if (i >= EMOTE_DEF_OBJ_ANIM_EYE && i <= EMOTE_DEF_OBJ_ANIM_EMERG_DLG) {
                if (entry->data.anim) {
                    if (entry->data.anim->cache) {
                        free(entry->data.anim->cache);
                    }
                    free(entry->data.anim);
                    entry->data.anim = NULL;
                }
            } else if (i == EMOTE_DEF_OBJ_ICON_STATUS || i == EMOTE_DEF_OBJ_ICON_CHARGE) {
                if (entry->data.img) {
                    if (entry->data.img->cache) {
                        free(entry->data.img->cache);
                    }
                    free(entry->data.img);
                    entry->data.img = NULL;
                }
            }
        }

        // Cleanup custom objects created by load_layouts
        emote_custom_obj_entry_t *custom_entry = handle->custom_objects;
        while (custom_entry) {
            emote_custom_obj_entry_t *next = custom_entry->next;
            if (custom_entry->obj) {
                gfx_object_delete(custom_entry->obj);
            }
            if (custom_entry->name) {
                free(custom_entry->name);
            }
            free(custom_entry);
            custom_entry = next;
        }
        handle->custom_objects = NULL;

        // Cleanup emergency dialog timer
        if (handle->dialog_timer) {
            gfx_timer_delete(handle->gfx_handle, handle->dialog_timer);
            handle->dialog_timer = NULL;
        }

        gfx_core_unlock(handle->gfx_handle);
    }

    // Cleanup semaphore for emergency dialog animation completion
    if (handle->emerg_dlg_done_event) {
        gfx_platform_event_delete(handle->emerg_dlg_done_event);
        handle->emerg_dlg_done_event = NULL;
    }

    // Cleanup emoji table (destroy and recreate to clear all entries)
    if (handle->emoji_table) {
        emote_assets_table_destroy(handle->emoji_table);
        handle->emoji_table = NULL;
    }

    // Cleanup icon table (destroy and recreate to clear all entries)
    if (handle->icon_table) {
        emote_assets_table_destroy(handle->icon_table);
        handle->icon_table = NULL;
    }

    // Release font cache
    if (handle->font_cache) {
        free(handle->font_cache);
        handle->font_cache = NULL;
    }

    // Cleanup font
    if (handle->gfx_font) {
        gfx_font_lv_delete(handle->gfx_font);
        handle->gfx_font = NULL;
    }

    GFX_LOGI(TAG, "Unload assets");
    return GFX_OK;

error:
    return ret;
}

gfx_err_t emote_get_icon_data_by_name(emote_handle_t handle, const char *name, icon_data_t **icon)
{
    gfx_err_t ret = GFX_OK;

    if (!handle || !name || !icon) {
        return GFX_ERR_INVALID_ARG;
    }
    ret = emote_find_data_by_key(handle, handle->icon_table, name, (void **)icon);
    return ret;
}

gfx_err_t emote_get_emoji_data_by_name(emote_handle_t handle, const char *name, emoji_data_t **emoji)
{
    gfx_err_t ret = GFX_OK;

    if (!handle || !name || !emoji) {
        return GFX_ERR_INVALID_ARG;
    }
    ret = emote_find_data_by_key(handle, handle->emoji_table, name, (void **)emoji);
    return ret;
}

emote_assets_addr_mode_t emote_get_assets_addr_mode(emote_handle_t handle)
{
    return emote_query_assets_addr_mode(handle);
}

gfx_err_t emote_mount_and_load_assets(emote_handle_t handle, const emote_data_t *data)
{
    gfx_err_t ret = GFX_OK;

    GFX_GOTO_ON_FALSE(handle && data, GFX_ERR_INVALID_ARG, error, TAG, "Invalid parameters");

    ret = emote_mount_assets(handle, data);
    GFX_GOTO_ON_FALSE(ret == GFX_OK, ret, error, TAG, "Failed to mount assets");

    ret = emote_load_assets(handle);
    GFX_GOTO_ON_FALSE(ret == GFX_OK, ret, error, TAG, "Failed to load assets data");

    return GFX_OK;

error:
    return ret;
}
