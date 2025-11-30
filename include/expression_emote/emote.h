/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ===== EVENT TYPE CONSTANTS =====
#define EMOTE_MGR_EVT_IDLE        "evt_idle"
#define EMOTE_MGR_EVT_SPEAK       "evt_speak"
#define EMOTE_MGR_EVT_LISTEN      "evt_listen"
#define EMOTE_MGR_EVT_SYS         "evt_sys"
#define EMOTE_MGR_EVT_SET         "evt_set"
#define EMOTE_MGR_EVT_BAT         "evt_bat"
#define EMOTE_MGR_EVT_QRCODE      "evt_qrcode"

// ===== OPAQUE HANDLE =====
typedef struct emote_s *emote_handle_t;

// ===== CONFIGURATION STRUCTURES =====
typedef enum {
    EMOTE_SOURCE_PATH = 0,
    EMOTE_SOURCE_PARTITION
} emote_source_type_t;

typedef struct {
    emote_source_type_t type;
    union {
        const char *path;
        const char *partition_label;
    } source;
} emote_data_t;

// ===== FLUSH READY CALLBACK =====
typedef void (*emote_flush_ready_cb_t)(int x_start, int y_start, int x_end, int y_end, const void *data, emote_handle_t manager);

typedef struct {
    struct {
        bool swap;
        bool double_buffer;
        bool buff_dma;
    } flags;
    struct {
        int h_res;
        int v_res;
        int fps;
    } gfx_emote;
    struct {
        size_t buf_pixels;
    } buffers;
    struct {
        int task_priority;
        int task_stack;
        int task_affinity;
        bool task_stack_in_ext;
    } task;
    emote_flush_ready_cb_t flush_cb;  // Flush ready callback (can be NULL)
} emote_config_t;

// ===== API FUNCTIONS =====

/**
 * @brief Initialize the emote manager
 * @param config Configuration structure
 * @return Handle to emote manager on success, NULL on failure
 */
emote_handle_t emote_init(const emote_config_t *config);

/**
 * @brief Deinitialize and cleanup the emote manager
 * @param handle Handle to emote manager
 * @return true on success, false on failure
 */
bool emote_deinit(emote_handle_t handle);

/**
 * @brief Check if manager is initialized
 * @param handle Handle to emote manager
 * @return true if initialized, false otherwise
 */
bool emote_is_initialized(emote_handle_t handle);

/**
 * @brief Set emoji animation on eye object
 * @param handle Handle to emote manager
 * @param emoji_name Name of the emoji animation
 * @return true on success, false on failure
 */
bool emote_set_anim_emoji(emote_handle_t handle, const char *emoji_name);

/**
 * @brief Set QR code data
 * @param handle Handle to emote manager
 * @param qrcode_text QR code text
 * @return true on success, false on failure
 */
bool emote_set_qrcode_data(emote_handle_t handle, const char *qrcode_text);

/**
 * @brief Set emergency dialog animation
 * @param handle Handle to emote manager
 * @param emoji_name Name of the emoji animation
 * @return true on success, false on failure
 */
bool emote_set_dialog_anim(emote_handle_t handle, const char *emoji_name);

/**
 * @brief Insert emergency dialog animation with auto-stop timer
 * @param handle Handle to emote manager
 * @param emoji_name Name of the emoji animation
 * @param duration_ms Duration in milliseconds before auto-stopping
 * @return true on success, false on failure
 */
bool emote_insert_anim_dialog(emote_handle_t handle, const char *emoji_name, uint32_t duration_ms);

/**
 * @brief Stop emergency dialog animation
 * @param handle Handle to emote manager
 * @return true on success, false on failure
 */
bool emote_stop_anim_dialog(emote_handle_t handle);

/**
 * @brief Set system event with message
 * @param handle Handle to emote manager
 * @param event Event type string
 * @param message Message string
 * @return true on success, false on failure
 */
bool emote_set_event_msg(emote_handle_t handle, const char *event, const char *message);

/**
 * @brief Wait for boot animation to complete
 * @param handle Handle to emote manager
 * @param delete_anim Whether to delete animation after completion
 * @return true on success, false on failure
 */
bool emote_wait_boot_anim_stop(emote_handle_t handle, bool delete_anim);

/**
 * @brief Load and start boot animation from source
 * @param handle Handle to emote manager
 * @param data Source data structure
 * @return true on success, false on failure
 */
bool emote_load_boot_anim_from_source(emote_handle_t handle, const emote_data_t *data);

/**
 * @brief Load assets from source
 * @param handle Handle to emote manager
 * @param data Source data structure
 * @return true on success, false on failure
 */
bool emote_load_assets_from_source(emote_handle_t handle, const emote_data_t *data);

/**
 * @brief Notify that flush operation is finished
 * @param handle Handle to emote manager
 * @return true on success, false on failure
 */
bool emote_notify_flush_finished(emote_handle_t handle);

#ifdef __cplusplus
}
#endif
