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

#include "expression_emote.h"
#include "gfx.h"

static const char *TAG = "expression_emote_test";

static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;

static void test_flush_callback(int x_start, int y_start, int x_end, int y_end, const void *data, emote_handle_t manager)
{
    if (manager) {
        emote_notify_flush_finished(manager);
    }
    esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_end, y_end, data);
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

TEST_CASE("Test boot animation load and play", "[emote_mgr][boot_anim]")
{
    emote_handle_t handle = init_emote();
    if (handle) {
        emote_data_t data = {
            .type = EMOTE_SOURCE_PARTITION,
            .source = {
                .partition_label = "anim_boot",
            },
        };
        emote_load_boot_anim_from_source(handle, &data);

        emote_wait_boot_anim_stop(handle, true);
        ESP_LOGI(TAG, "Boot animation completed");

        cleanup_emote(handle);
    }
}

// TEST_CASE("Test assets load and play", "[emote_mgr][assets]")
void test_assets(void)
{
    emote_handle_t handle = init_emote();
    if (handle) {
        emote_data_t data = {
            .type = EMOTE_SOURCE_PARTITION,
            .source = {
                .partition_label = "anim_icon",
            },
        };
        emote_load_assets_from_source(handle, &data);

        emote_set_event_msg(handle, EMOTE_MGR_EVT_LISTEN, NULL);
        vTaskDelay(pdMS_TO_TICKS(3 * 1000));

        emote_set_event_msg(handle, EMOTE_MGR_EVT_SPEAK, "你好，我是 esp_emote_expression，我是 Brookesia！");
        vTaskDelay(pdMS_TO_TICKS(3 * 1000));

        emote_set_event_msg(handle, EMOTE_MGR_EVT_SPEAK, "Hello, I'm esp_emote_expression, I'm Brookesia!");
        vTaskDelay(pdMS_TO_TICKS(3 * 1000));

        emote_set_anim_emoji(handle, "happy");
        vTaskDelay(pdMS_TO_TICKS(3 * 1000));

        emote_set_anim_emoji(handle, "sad");
        vTaskDelay(pdMS_TO_TICKS(3 * 1000));

        emote_insert_anim_dialog(handle, "angry", 5 * 1000);
        vTaskDelay(pdMS_TO_TICKS(3 * 1000));

        emote_set_qrcode_data(handle, "https://www.esp32.com");
        vTaskDelay(pdMS_TO_TICKS(3 * 1000));

        emote_set_event_msg(handle, EMOTE_MGR_EVT_IDLE, NULL);
        emote_set_event_msg(handle, EMOTE_MGR_EVT_BAT, "0,50");
        vTaskDelay(pdMS_TO_TICKS(3 * 1000));

        emote_set_event_msg(handle, EMOTE_MGR_EVT_BAT, "1,100");
        vTaskDelay(pdMS_TO_TICKS(3 * 1000));
        
        // 1. Create custom label with default properties
        // Note: emote_create_obj_by_type automatically sets default properties:
        // - Font: font_maison_neue_book_26
        // - Color: White (0xFFFFFF)
        // - Size: 100x25 (EMOTE_DEFAULT_LABEL_WIDTH x EMOTE_DEFAULT_LABEL_HEIGHT)
        // - Text align: Center
        // - Long mode: Scroll
        // - Scroll speed: 10ms per pixel
        emote_set_event_msg(handle, EMOTE_MGR_EVT_SPEAK, "");
        gfx_obj_t *custom_label_1 = emote_create_obj_by_type(handle, "custom_label_1", "label");
        if (custom_label_1) {
            gfx_label_set_text(custom_label_1, "Custom Label 1");
            // Optionally customize properties (these override defaults):
            // gfx_label_set_color(custom_label_1, GFX_COLOR_HEX(0xFF0000));  // Red text
            // gfx_label_set_font(custom_label_1, (void *)&font_maison_neue_book_12);  // Smaller font
            // gfx_obj_set_size(custom_label_1, 200, 30);  // Custom size
            // gfx_obj_align(custom_label_1, GFX_ALIGN_CENTER, 0, 0);  // Center alignment
        }
        
        vTaskDelay(pdMS_TO_TICKS(2 * 1000));
        
        // 3. Modify toast element label
        gfx_obj_t *toast_label = emote_get_obj_by_name(handle, "toast_label");
        if (toast_label) {
            gfx_label_set_text(toast_label, "Toast Label Updated");
            ESP_LOGI(TAG, "Updated toast_label text to: 'Toast Label Updated'");
        } else {
            ESP_LOGW(TAG, "toast_label not found");
        }
        
        vTaskDelay(pdMS_TO_TICKS(2 * 1000));

        cleanup_emote(handle);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Expression Emote test");
    // unity_run_menu();
    test_assets();
}
