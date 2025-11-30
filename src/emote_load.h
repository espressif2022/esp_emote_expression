/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "expression_emote.h"
#include "esp_mmap_assets.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
typedef struct assets_hash_table_s assets_hash_table_t;

// ===== Hash Table Management =====
/**
 * @brief  Create a new assets hash table
 *
 * @return
 *       - Pointer to hash table  On success
 *       - NULL                  Fail to create hash table
 */
assets_hash_table_t *emote_assets_table_create(void);

/**
 * @brief  Destroy and free assets hash table
 *
 * @param[in]  ht  Hash table to destroy
 */
void emote_assets_table_destroy(assets_hash_table_t *ht);

// ===== Asset Data Acquisition =====
/**
 * @brief  Acquire asset data with caching support
 *
 * @param[in]   handle        Emote handle
 * @param[in]   asset_handle  Memory-mapped assets handle
 * @param[in]   data_ref      Reference to data
 * @param[in]   size          Size of data
 * @param[out]  output_ptr    Output pointer to store cached data pointer
 *
 * @return
 *       - Pointer to data  On success
 *       - NULL             Fail to acquire data
 */
const void *emote_acquire_data(emote_handle_t handle, mmap_assets_handle_t asset_handle,
                               const void *data_ref, size_t size, void **output_ptr);

/**
 * @brief  Find asset data by name from memory-mapped assets
 *
 * @param[in]   handle  Memory-mapped assets handle
 * @param[in]   name    Asset name to search for
 * @param[out]  data    Output pointer to store data pointer
 * @param[out]  size    Output pointer to store data size
 *
 * @return
 *       - true   Asset found
 *       - false  Asset not found
 */
bool emote_find_data_by_name(mmap_assets_handle_t handle, const char *name,
                             const uint8_t **data, size_t *size);

/**
 * @brief  Find asset data by key from hash table
 *
 * @param[in]   handle  Emote handle
 * @param[in]   ht      Hash table to search
 * @param[in]   key     Key to search for
 * @param[out]  result  Output pointer to store result
 *
 * @return
 *       - true   Key found
 *       - false  Key not found
 */
bool emote_find_data_by_key(emote_handle_t handle, assets_hash_table_t *ht,
                            const char *key, void **result);

#ifdef __cplusplus
}
#endif
