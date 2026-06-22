/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"
#if !CONFIG_IDF_TARGET_ESP32S31
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#endif

static const char *TAG = "expression_emote_common";

#if CONFIG_IDF_TARGET_ESP32S31
#define TEST_APP_HAS_BSP 0
#define TEST_APP_LCD_H_RES 320
#define TEST_APP_LCD_V_RES 240
#else
#define TEST_APP_HAS_BSP 1
#define TEST_APP_LCD_H_RES BSP_LCD_H_RES
#define TEST_APP_LCD_V_RES BSP_LCD_V_RES
#endif

#if TEST_APP_HAS_BSP
static esp_lcd_panel_io_handle_t s_io_handle = NULL;
static esp_lcd_panel_handle_t s_panel_handle = NULL;
#endif

static void test_flush_callback(int x_start, int y_start, int x_end, int y_end, const void *data,
                                emote_handle_t handle)
{
#if TEST_APP_HAS_BSP
    (void)handle;
    esp_lcd_panel_draw_bitmap(s_panel_handle, x_start, y_start, x_end, y_end, data);
#else
    (void)x_start;
    (void)y_start;
    (void)x_end;
    (void)y_end;
    (void)data;
    if (handle) {
        emote_notify_flush_finished(handle);
    }
#endif
}

#if TEST_APP_HAS_BSP && CONFIG_IDF_TARGET_ESP32S3
static bool test_flush_io_ready_callback(esp_lcd_panel_io_handle_t panel_io,
                                         esp_lcd_panel_io_event_data_t *edata,
                                         void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    emote_handle_t handle = (emote_handle_t)user_ctx;
    if (handle) {
        emote_notify_flush_finished(handle);
    }
    return true;
}

#elif TEST_APP_HAS_BSP && CONFIG_IDF_TARGET_ESP32P4
static bool test_flush_dpi_panel_ready_callback(esp_lcd_panel_handle_t panel_io,
                                                esp_lcd_dpi_panel_event_data_t *edata,
                                                void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    emote_handle_t handle = (emote_handle_t)user_ctx;
    if (handle) {
        emote_notify_flush_finished(handle);
    }
    return true;
}
#endif

static void test_update_callback(gfx_display_event_t event, const void *obj, emote_handle_t handle)
{
    if (handle) {
        gfx_object_t *wait_obj = emote_get_obj_by_name(handle, EMT_DEF_ELEM_EMERG_DLG);
        if (wait_obj == obj && event == GFX_DISPLAY_EVENT_ALL_FRAME_DONE) {
            ESP_LOGI(TAG, "Emergency dialog done: %p, event: %d", obj, event);
        }
    }
}

static void test_app_display_init(void)
{
#if TEST_APP_HAS_BSP && CONFIG_IDF_TARGET_ESP32S3
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = (BSP_LCD_H_RES * 20) * sizeof(uint16_t),
    };
    bsp_display_new(&bsp_disp_cfg, &s_panel_handle, &s_io_handle);
#elif TEST_APP_HAS_BSP && CONFIG_IDF_TARGET_ESP32P4
    const bsp_display_config_t bsp_disp_cfg = {
        .hdmi_resolution = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .dsi_bus = {
            .phy_clk_src = 0,
            .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
        },
    };
    bsp_display_new(&bsp_disp_cfg, &s_panel_handle, &s_io_handle);
#endif
#if TEST_APP_HAS_BSP
    esp_lcd_panel_disp_on_off(s_panel_handle, true);
    bsp_display_backlight_on();
#else
    ESP_LOGI(TAG, "Skip BSP display initialization for ESP32-S31");
#endif
}

static emote_config_t test_app_get_default_emote_config(void)
{
    emote_config_t config = {
        .flags = {
#if CONFIG_IDF_TARGET_ESP32S3
            .swap = true,
#elif CONFIG_IDF_TARGET_ESP32P4
            .swap = false,
#endif
            .double_buffer = true,
            .buff_dma = true,
        },
        .gfx_emote = {
            .h_res = TEST_APP_LCD_H_RES,
            .v_res = TEST_APP_LCD_V_RES,
            .fps = 30,
        },
        .buffers = {
            .buf_pixels = TEST_APP_LCD_H_RES * 16,
        },
        .task = {
            .task_priority = 5,
            .task_stack = 6 * 1024,
            .task_affinity = -1,
            .task_stack_in_ext = false,
        },
        .flush_cb = test_flush_callback,
        .update_cb = test_update_callback,
    };
    return config;
}

static void test_app_register_flush_ready_callback(emote_handle_t handle)
{
#if TEST_APP_HAS_BSP && CONFIG_IDF_TARGET_ESP32S3
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = test_flush_io_ready_callback,
    };
    esp_lcd_panel_io_register_event_callbacks(s_io_handle, &cbs, handle);
#elif TEST_APP_HAS_BSP && CONFIG_IDF_TARGET_ESP32P4
    esp_lcd_dpi_panel_event_callbacks_t cbs = {0};
    cbs.on_color_trans_done = test_flush_dpi_panel_ready_callback;
    esp_lcd_dpi_panel_register_event_callbacks(s_panel_handle, &cbs, handle);
#endif
}

emote_handle_t test_app_emote_create(void)
{
    ESP_LOGI(TAG, "=== Create ===");

    test_app_display_init();
    emote_config_t config = test_app_get_default_emote_config();
    emote_handle_t handle = emote_init(&config);
    TEST_ASSERT_NOT_NULL(handle);
    if (handle) {
        TEST_ASSERT_TRUE(emote_is_initialized(handle));
        test_app_register_flush_ready_callback(handle);
    }
    return handle;
}

void test_app_emote_delete(emote_handle_t handle)
{
    ESP_LOGI(TAG, "=== Cleanup ===");
    if (handle) {
        bool deinit_result = emote_deinit(handle);
        TEST_ASSERT_TRUE(deinit_result);
    }

#if TEST_APP_HAS_BSP && CONFIG_IDF_TARGET_ESP32S3
    if (s_panel_handle) {
        esp_lcd_panel_del(s_panel_handle);
        s_panel_handle = NULL;
    }
    if (s_io_handle) {
        esp_lcd_panel_io_del(s_io_handle);
        s_io_handle = NULL;
    }
    spi_bus_free(BSP_LCD_SPI_NUM);
#elif TEST_APP_HAS_BSP && CONFIG_IDF_TARGET_ESP32P4
    bsp_display_delete();
    s_panel_handle = NULL;
    s_io_handle = NULL;
#endif
    test_app_wait_ms(1000);
}

void test_app_wait_ms(uint32_t delay_ms)
{
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}
