/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"
#include "unity_test_utils.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#if CONFIG_EXAMPLE_EXPRESSIONS_ENABLE_EMOTE
#include "dirent.h"
#include "expression_emote.h"
#include "gfx.h"

static const char *TAG = "expression_emote_test";

static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;

static gfx_image_dsc_t img_dsc = {0};

static void test_flush_callback(int x_start, int y_start, int x_end, int y_end, const void *data, emote_handle_t handle)
{
    if (handle) {
        emote_notify_flush_finished(handle);
    }
    esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_end, y_end, data);
}

static void test_update_callback(gfx_player_event_t event, const void *obj, emote_handle_t handle)
{
    if (handle) {
        gfx_obj_t *wait_obj = emote_get_obj_by_name(handle, EMT_DEF_ELEM_EMERG_DLG);
        if (wait_obj == obj && event == GFX_PLAYER_EVENT_ALL_FRAME_DONE) {
            ESP_LOGI(TAG, "It's an Emergency dialog: %p, event: %d", obj, event);
        }
    }
}

// Initialize display
static void init_display(void)
{
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = (BSP_LCD_H_RES * 20) * sizeof(uint16_t),
    };
    bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);
    bsp_display_backlight_on();
}

// Get default emote configuration
static emote_config_t get_default_emote_config(void)
{
    emote_config_t config = {
        .flags = {
            .swap = true,
            .double_buffer = true,
            .buff_dma = false,
        },
        .gfx_emote = {
            .h_res = BSP_LCD_H_RES,
            .v_res = BSP_LCD_V_RES,
            .fps = 30,
        },
        .buffers = {
            .buf_pixels = BSP_LCD_H_RES * 16,
        },
        .task = {
            .task_priority = 5,
            .task_stack = 4096,
            .task_affinity = -1,
            .task_stack_in_ext = false,
        },
        .flush_cb = test_flush_callback,
        .update_cb = test_update_callback,
    };
    return config;
}

// Initialize emote with display
static emote_handle_t init_emote(void)
{
    init_display();
    emote_config_t config = get_default_emote_config();
    emote_handle_t handle = emote_init(&config);
    TEST_ASSERT_NOT_NULL(handle);
    if (handle) {
        TEST_ASSERT_TRUE(emote_is_initialized(handle));
    }
    return handle;
}

static void cleanup_emote(emote_handle_t handle)
{
    ESP_LOGI(TAG, "=== Cleanup display and graphics ===");

    if (handle) {
        bool deinit_result = emote_deinit(handle);
        TEST_ASSERT_TRUE(deinit_result);
    }

    if (panel_handle) {
        esp_lcd_panel_del(panel_handle);
    }
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);
    }
    spi_bus_free(BSP_LCD_SPI_NUM);

    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void test_emote_basic(emote_handle_t handle)
{
    if (!handle) {
        return;
    }

    ESP_LOGI(TAG, "Insert anim");
    emote_insert_anim_dialog(handle, "angry", 5 * 1000);
    esp_err_t ret = emote_wait_emerg_dlg_done(handle, 10 * 1000); // 10 second timeout
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Emergency dialog animation completed");
    } else {
        ESP_LOGE(TAG, "Wait for emergency dialog failed: %s", esp_err_to_name(ret));
    }

    emote_set_event_msg(handle, EMOTE_MGR_EVT_LISTEN, NULL);
    vTaskDelay(pdMS_TO_TICKS(3 * 1000));

    emote_set_event_msg(handle, EMOTE_MGR_EVT_SPEAK, "你好，我是 esp_emote_expression，我是 Brookesia！");
    vTaskDelay(pdMS_TO_TICKS(3 * 1000));

    emote_set_event_msg(handle, EMOTE_MGR_EVT_SPEAK, "Hello, I'm esp_emote_expression, I'm Brookesia!");
    vTaskDelay(pdMS_TO_TICKS(3 * 1000));

    emote_set_anim_emoji(handle, "happy");
    vTaskDelay(pdMS_TO_TICKS(3 * 1000));

    emote_set_qrcode_data(handle, "https://www.esp32.com");
    vTaskDelay(pdMS_TO_TICKS(3 * 1000));

    emote_set_event_msg(handle, EMOTE_MGR_EVT_IDLE, NULL);
    emote_set_event_msg(handle, EMOTE_MGR_EVT_BAT, "0,50");
    vTaskDelay(pdMS_TO_TICKS(3 * 1000));

    emote_set_event_msg(handle, EMOTE_MGR_EVT_BAT, "1,100");
    vTaskDelay(pdMS_TO_TICKS(3 * 1000));
}

static void test_emote_custom(emote_handle_t handle)
{
    if (!handle) {
        return;
    }


    gfx_obj_t *custom_label = emote_create_obj_by_type(handle, EMOTE_OBJ_TYPE_LABEL, "custom_label");
    if (custom_label) {
        emote_lock(handle);
        gfx_label_set_text(custom_label, "Custom Label");
        gfx_label_set_color(custom_label, GFX_COLOR_HEX(0xFF0000));
        gfx_obj_set_size(custom_label, 200, 30);
        gfx_obj_align(custom_label, GFX_ALIGN_CENTER, 0, 0);
        emote_unlock(handle);
    }

    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    // Modify toast element label
    emote_set_event_msg(handle, EMOTE_MGR_EVT_SPEAK, "");
    gfx_obj_t *toast_label = emote_get_obj_by_name(handle, "toast_label");
    if (toast_label) {
        emote_lock(handle);
        gfx_label_set_text(toast_label, "Toast Label Updated");
        emote_unlock(handle);
    } else {
        ESP_LOGW(TAG, "toast_label not found");
    }

    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    icon_data_t *icon_data = NULL;
    emote_get_icon_data_by_name(handle, "icon_tips", &icon_data);
    memcpy(&img_dsc.header, icon_data->data, sizeof(gfx_image_header_t));
    img_dsc.data = (const uint8_t *)icon_data->data + sizeof(gfx_image_header_t);
    img_dsc.data_size = icon_data->size - sizeof(gfx_image_header_t);

    gfx_obj_t *custom_img = emote_create_obj_by_type(handle, EMOTE_OBJ_TYPE_IMAGE, "custom_image");
    emote_lock(handle);
    gfx_img_set_src(custom_img, &img_dsc);
    gfx_obj_set_visible(custom_img, true);
    gfx_obj_align(custom_img, GFX_ALIGN_CENTER, 0, 50);
    emote_unlock(handle);

    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    emoji_data_t *emoji_data = NULL;
    emote_get_emoji_data_by_name(handle, "happy", &emoji_data);

    gfx_obj_t *custom_anim = emote_create_obj_by_type(handle, EMOTE_OBJ_TYPE_ANIM, "custom_anim");
    emote_lock(handle);
    ESP_LOGI(TAG, "Animation data: %p, size: %d", emoji_data->data, emoji_data->size);
    ESP_LOGI(TAG, "Animation fps: %d, loop: %d", emoji_data->fps, emoji_data->loop);
    gfx_anim_set_src(custom_anim, emoji_data->data, emoji_data->size);
    gfx_anim_set_segment(custom_anim, 0, 0xFFFF, emoji_data->fps, emoji_data->loop);
    gfx_anim_start(custom_anim);
    gfx_obj_set_visible(custom_anim, true);
    gfx_obj_align(custom_anim, GFX_ALIGN_CENTER, 0, 0);
    emote_unlock(handle);

    vTaskDelay(pdMS_TO_TICKS(3 * 1000));
}

TEST_CASE("Test basic elements", "[partition][flash mmap][basic]")
{
    emote_handle_t handle = init_emote();
    if (handle) {
        emote_data_t data = {
            .type = EMOTE_SOURCE_PARTITION,
            .source = {
                .partition_label = "anim_icon",
            },
            .flags = {
                .mmap_enable = true,
            },
        };
        ESP_LOGI(TAG, "Assets loaded from partition");
        emote_mount_and_load_assets(handle, &data);

        test_emote_basic(handle);

        cleanup_emote(handle);
    }
}

TEST_CASE("Test basic elements", "[partition][flash read][basic]")
{
    emote_handle_t handle = init_emote();
    if (handle) {
        emote_data_t data = {
            .type = EMOTE_SOURCE_PARTITION,
            .source = {
                .partition_label = "anim_icon",
            },
            .flags = {
                .mmap_enable = false,
            },
        };
        ESP_LOGI(TAG, "Assets loaded from partition");
        emote_mount_and_load_assets(handle, &data);

        test_emote_basic(handle);

        cleanup_emote(handle);
    }
}

TEST_CASE("Test basic elements", "[path][flash read][basic]")
{
    bsp_spiffs_mount();

    struct dirent *p_dirent = NULL;
    DIR *p_dir_stream = opendir(BSP_SPIFFS_MOUNT_POINT);

    /* Scan files in storage */
    while (true) {
        p_dirent = readdir(p_dir_stream);
        if (p_dirent == NULL) {
            break;
        }
        ESP_LOGI(TAG, "File: %s", p_dirent->d_name);
    }
    closedir(p_dir_stream);

    emote_handle_t handle = init_emote();
    if (handle) {
        emote_data_t data = {
            .type = EMOTE_SOURCE_PATH,
            .source = {
                .path = BSP_SPIFFS_MOUNT_POINT "/asset_test.bin",
            },
        };
        ESP_LOGI(TAG, "Assets loaded from path:%s", data.source.path);
        emote_mount_and_load_assets(handle, &data);

        test_emote_basic(handle);

        cleanup_emote(handle);
    }

    bsp_spiffs_unmount();
}

TEST_CASE("Test custom elements", "[partition][flash mmap][custom]")
{
    emote_handle_t handle = init_emote();
    if (handle) {
        emote_data_t data = {
            .type = EMOTE_SOURCE_PARTITION,
            .source = {
                .partition_label = "anim_icon",
            },
            .flags = {
                .mmap_enable = true,
            },
        };
        ESP_LOGI(TAG, "Assets loaded from partition");
        emote_mount_and_load_assets(handle, &data);

        test_emote_custom(handle);

        cleanup_emote(handle);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Expression Emote test");
    unity_run_menu();
}
#else
void app_main(void)
{
    ESP_LOGI("", "Emote is not enabled, skipping test");
}
#endif
