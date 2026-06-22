/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common.h"

#include <string.h>
#include "dirent.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "unity.h"
#if !CONFIG_IDF_TARGET_ESP32S31
#include "bsp/esp-bsp.h"
#endif

static const char *TAG = "expression_emote_test";

static gfx_image_dsc_t s_img_dsc = {0};

static void test_emote_basic(emote_handle_t handle)
{
    if (!handle) {
        return;
    }

    ESP_LOGI(TAG, "Insert anim: %s", "angry");
    emote_insert_anim_dialog(handle, "angry", 5 * 1000);
    esp_err_t ret = emote_wait_emerg_dlg_done(handle, 10 * 1000);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Emergency dialog done");
    } else {
        ESP_LOGE(TAG, "Wait failed: %s", esp_err_to_name(ret));
    }

    emote_set_event_msg(handle, EMOTE_MGR_EVT_LISTEN, NULL);
    test_app_wait_ms(3 * 1000);

    emote_set_event_msg(handle, EMOTE_MGR_EVT_SPEAK, "你好，我是 esp_emote_expression，我是 Brookesia！");
    test_app_wait_ms(2 * 1000);

    emote_set_event_msg(handle, EMOTE_MGR_EVT_SPEAK, "Hello, I'm esp_emote_expression, I'm Brookesia!");
    test_app_wait_ms(2 * 1000);

    emote_set_anim_emoji(handle, "happy");
    test_app_wait_ms(3 * 1000);

    ESP_LOGI(TAG, "Insert anim: %s", "angry");
    emote_insert_anim_dialog(handle, "angry", 5 * 1000);
    test_app_wait_ms(3 * 1000);
    ESP_LOGI(TAG, "Stop anim");
    emote_stop_anim_dialog(handle);

    emote_set_qrcode_data(handle, "https://www.esp32.com");
    test_app_wait_ms(3 * 1000);

    emote_set_event_msg(handle, EMOTE_MGR_EVT_BAT, "0,50");
    test_app_wait_ms(2 * 1000);
    emote_set_event_msg(handle, EMOTE_MGR_EVT_IDLE, NULL);
    test_app_wait_ms(2 * 1000);

    emote_set_event_msg(handle, EMOTE_MGR_EVT_BAT, "1,100");
    test_app_wait_ms(2 * 1000);

    ESP_LOGI(TAG, "Set off event");
    emote_set_event_msg(handle, EMOTE_MGR_EVT_OFF, NULL);
    test_app_wait_ms(3 * 1000);

    emote_set_event_msg(handle, EMOTE_MGR_EVT_LISTEN, NULL);
    test_app_wait_ms(3 * 1000);

    ESP_LOGI(TAG, "Set face visible: false");
    emote_set_anim_visible(handle, false);
    test_app_wait_ms(3 * 1000);

    ESP_LOGI(TAG, "Set face visible: true");
    emote_set_anim_visible(handle, true);
    test_app_wait_ms(3 * 1000);

    ESP_LOGI(TAG, "Set listen anim visible: false");
    emote_set_obj_visible(handle, EMT_DEF_ELEM_LISTEN_ANIM, false);
    test_app_wait_ms(3 * 1000);
}

static void test_emote_custom(emote_handle_t handle)
{
    if (!handle) {
        return;
    }

    gfx_object_t *custom_label = emote_create_obj_by_type(handle, EMOTE_OBJ_TYPE_LABEL, "custom_label");
    if (custom_label) {
        emote_lock(handle);
        gfx_label_set_text(custom_label, "Custom Label");
        gfx_label_set_color(custom_label, GFX_COLOR_HEX(0xFF0000));
        gfx_object_set_size(custom_label, 200, 30);
        gfx_object_align(custom_label, GFX_ALIGN_CENTER, 0, 0);
        emote_unlock(handle);
    }

    test_app_wait_ms(2 * 1000);

    emote_set_event_msg(handle, EMOTE_MGR_EVT_SPEAK, "");
    gfx_object_t *toast_label = emote_get_obj_by_name(handle, "toast_label");
    if (toast_label) {
        emote_lock(handle);
        gfx_label_set_text(toast_label, "Toast Label Updated");
        emote_unlock(handle);
    } else {
        ESP_LOGW(TAG, "toast_label not found");
    }

    test_app_wait_ms(2 * 1000);

    icon_data_t *icon_data = NULL;
    emote_get_icon_data_by_name(handle, "icon_tips", &icon_data);
    memcpy(&s_img_dsc.header, icon_data->data, sizeof(gfx_image_header_t));
    s_img_dsc.data = (const uint8_t *)icon_data->data + sizeof(gfx_image_header_t);
    s_img_dsc.data_size = icon_data->size - sizeof(gfx_image_header_t);

    gfx_object_t *custom_img = emote_create_obj_by_type(handle, EMOTE_OBJ_TYPE_IMAGE, "custom_image");
    emote_lock(handle);
    const gfx_image_src_t img_src = {
        .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
        .data = &s_img_dsc,
        .data_len = s_img_dsc.data_size,
    };
    gfx_image_set_source_desc(custom_img, &img_src);
    gfx_object_set_visible(custom_img, true);
    gfx_object_align(custom_img, GFX_ALIGN_CENTER, 0, 50);
    emote_unlock(handle);

    test_app_wait_ms(2 * 1000);

    emoji_data_t *emoji_data = NULL;
    emote_get_emoji_data_by_name(handle, "happy", &emoji_data);

    gfx_object_t *custom_anim = emote_create_obj_by_type(handle, EMOTE_OBJ_TYPE_ANIM, "custom_anim");
    emote_lock(handle);
    const gfx_anim_src_t anim_src = {
        .type = GFX_ANIM_SRC_TYPE_MEMORY,
        .data = emoji_data->data,
        .data_len = emoji_data->size,
    };
    gfx_anim_set_src_desc(custom_anim, &anim_src);
    gfx_anim_set_segment(custom_anim, 0, 0xFFFF, emoji_data->fps, emoji_data->loop);
    gfx_anim_start(custom_anim);
    gfx_object_set_visible(custom_anim, true);
    gfx_object_align(custom_anim, GFX_ALIGN_CENTER, 0, 0);
    emote_unlock(handle);

    test_app_wait_ms(3 * 1000);
}

TEST_CASE("Test basic elements", "[partition][flash mmap][basic]")
{
    emote_handle_t handle = test_app_emote_create();
    if (handle) {
        emote_data_t data = {
            .type = EMOTE_SOURCE_PARTITION,
            .source = {
                .partition_label = TEST_APP_ASSETS_PARTITION,
            },
            .flags = {
                .mmap_enable = true,
            },
        };
        ESP_LOGI(TAG, "Assets loaded from partition");
        emote_mount_and_load_assets(handle, &data);

        test_emote_basic(handle);

        test_app_emote_delete(handle);
    }
}

TEST_CASE("Test basic elements", "[partition][flash read][basic]")
{
    emote_handle_t handle = test_app_emote_create();
    if (handle) {
        emote_data_t data = {
            .type = EMOTE_SOURCE_PARTITION,
            .source = {
                .partition_label = TEST_APP_ASSETS_PARTITION,
            },
            .flags = {
                .mmap_enable = false,
            },
        };
        ESP_LOGI(TAG, "Assets loaded from partition");
        emote_mount_and_load_assets(handle, &data);

        test_emote_basic(handle);

        test_app_emote_delete(handle);
    }
}

TEST_CASE("Test basic elements", "[path][flash read][basic]")
{
#if CONFIG_IDF_TARGET_ESP32S31
    TEST_IGNORE_MESSAGE("ESP32-S31 has no BSP SPIFFS helpers yet");
#else
    bsp_spiffs_mount();

    struct dirent *p_dirent = NULL;
    DIR *p_dir_stream = opendir(BSP_SPIFFS_MOUNT_POINT);

    while (true) {
        p_dirent = readdir(p_dir_stream);
        if (p_dirent == NULL) {
            break;
        }
        ESP_LOGI(TAG, "File: %s", p_dirent->d_name);
    }
    closedir(p_dir_stream);

    emote_handle_t handle = test_app_emote_create();
    if (handle) {
        emote_data_t data = {
            .type = EMOTE_SOURCE_PATH,
            .source = {
                .path = BSP_SPIFFS_MOUNT_POINT "/esp32_s3_assets.bin",
            },
        };
        ESP_LOGI(TAG, "Assets loaded from path:%s", data.source.path);
        emote_mount_and_load_assets(handle, &data);

        test_emote_basic(handle);

        test_app_emote_delete(handle);
    }

    bsp_spiffs_unmount();
#endif
}

TEST_CASE("Test custom elements", "[partition][flash mmap][custom]")
{
    emote_handle_t handle = test_app_emote_create();
    if (handle) {
        emote_data_t data = {
            .type = EMOTE_SOURCE_PARTITION,
            .source = {
                .partition_label = TEST_APP_ASSETS_PARTITION,
            },
            .flags = {
                .mmap_enable = true,
            },
        };
        ESP_LOGI(TAG, "Assets loaded from partition");
        emote_mount_and_load_assets(handle, &data);

        test_emote_custom(handle);

        test_app_emote_delete(handle);
    }
}

TEST_CASE("Test mount and load assets", "[partition][flash mmap][custom]")
{
    emote_data_t data = {
        .type = EMOTE_SOURCE_PARTITION,
        .source = {
            .partition_label = TEST_APP_ASSETS_PARTITION,
        },
        .flags = {
            .mmap_enable = true,
        },
    };

    emote_handle_t handle = test_app_emote_create();

    if (handle) {
        emote_set_event_msg(handle, EMOTE_MGR_EVT_SYS, "default_label");
        test_app_wait_ms(2 * 1000);

        emote_mount_assets(handle, &data);

        int free_memory_1 = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        printf("Free memory: %d, %s\n",
               heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
               "before load assets");
        emote_load_assets(handle);

        test_app_wait_ms(1 * 1000);

        emote_unload_assets(handle);

        test_app_wait_ms(1 * 1000);
        int free_memory_2 = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        printf("Free memory: %d, %s, delta: %d\n",
               heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
               "after unload assets", free_memory_2 - free_memory_1);

        TEST_ASSERT_EQUAL((free_memory_2 - free_memory_1) > 0, true);
        emote_unmount_assets(handle);
    }

    emote_mount_and_load_assets(handle, &data);
    test_emote_custom(handle);
    printf("Free memory: %d, %s\n",
           heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
           "after load assets");

    test_app_emote_delete(handle);
}
