/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "expression_emote.h"
#include "emote_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===== Hash Table Management =====
/**
 * @brief  Create a new assets hash table
 *
 * @param[in]  name  Name of the hash table (for logging/debugging, can be NULL)
 *
 * @return
 *       - Pointer to hash table  On success
 *       - NULL                  Fail to create hash table
 */
assets_hash_table_t *emote_assets_table_create(const char *name);

/**
 * @brief  Destroy and free assets hash table
 *
 * @param[in]  ht  Hash table to destroy
 */
void emote_assets_table_destroy(assets_hash_table_t *ht);

// ===== Asset Data Acquisition =====
/**
 * @brief  Acquire asset data with optional caller-owned cache.
 *
 * Behavior depends on mount mode (see emote_get_assets_addr_mode()):
 * - EMOTE_ASSETS_ADDR_DIRECT: borrow flash-backed @p data_ref (no copy).
 * - EMOTE_ASSETS_ADDR_COPY: borrow heap @p data_ref (no extra copy).
 * When @p output_ptr is set, any previous *output_ptr buffer is freed first.
 *
 * @param[in]   handle        Emote handle
 * @param[in]   data_ref      Reference to data
 * @param[in]   size          Size of data
 * @param[out]  output_ptr    Optional cache slot (freed/replaced when copy path runs)
 *
 * @return
 *       - Pointer to data  On success
 *       - NULL             Fail to acquire data
 */
const void *emote_acquire_data(emote_handle_t handle, const void *data_ref, size_t size, void **output_ptr);

#ifdef __cplusplus
}
#endif
