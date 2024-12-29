/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "clock.h"
#include "spinlock.h"
#include "xalloc.h"
#include "transaction.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "fiber/fiber.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"
#include "module/redis/module_redis_connection.h"

#define TAG "module_redis_command_lcs"

static inline __attribute__((always_inline)) size_t lcsmap_calculate_offset(
        size_t length1,
        uint32_t offset1,
        uint32_t offset2) {
    size_t lcsmap_offset = offset1 + (offset2 * (length1 + 1));
    return lcsmap_offset;
}

static inline __attribute__((always_inline)) uint32_t lcsmap_get(
        uint32_t *lcsmap,
        size_t length1,
        uint32_t offset1,
        uint32_t offset2) {
    return  lcsmap[lcsmap_calculate_offset(length1, offset1, offset2)];
}

static inline __attribute__((always_inline)) void lcsmap_set(
        uint32_t *lcsmap,
        size_t length1,
        uint32_t offset1,
        uint32_t offset2,
        uint32_t value) {
    lcsmap[lcsmap_calculate_offset(length1, offset1, offset2)] = value;
}

uint32_t *lcsmap_build(
        storage_db_t *db,
        storage_db_chunk_sequence_t *value_1,
        storage_db_chunk_sequence_t *value_2) {
    int32_t value_1_chunk_index = 0, value_1_chunk_offset = 0;
    int32_t value_2_chunk_index = 0, value_2_chunk_offset = 0;
    storage_db_chunk_info_t *value_1_chunk_info = NULL, *value_2_chunk_info = NULL;
    char *value_1_chunk_data = NULL, *value_2_chunk_data = NULL;
    bool value_1_chunk_data_allocated_new = false, value_2_chunk_data_allocated_new = false;

    // Initialize the generalized trees suffixes array
    uint32_t *lcsmap = xalloc_alloc_zero(sizeof(uint32_t) * (value_1->size + 1) * (value_2->size + 1));

    // Loop over the characters to build the tree
    value_1_chunk_index = -1;
    value_1_chunk_offset = -1;
    for(uint32_t value_1_char_index = 0; value_1_char_index <= value_1->size; value_1_char_index++) {
        value_1_chunk_offset++;
        if (unlikely(value_1_chunk_index == -1 || value_1_chunk_offset >= value_1_chunk_info->chunk_length)) {
            if (unlikely(value_1_chunk_data_allocated_new)) {
                xalloc_free(value_1_chunk_data);
                value_1_chunk_data_allocated_new = false;
            }

            value_1_chunk_index++;
            value_1_chunk_info = storage_db_chunk_sequence_get(value_1, value_1_chunk_index);
            value_1_chunk_data = storage_db_get_chunk_data(
                    db,
                    value_1_chunk_info,
                    &value_1_chunk_data_allocated_new);

            if (value_1_chunk_index == 0) {
                value_1_chunk_offset = -1;
            } else {
                value_1_chunk_offset = 0;
            }
        }

        value_2_chunk_index = -1;
        value_2_chunk_offset = -1;
        for(uint32_t value_2_char_index = 0; value_2_char_index <= value_2->size; value_2_char_index++) {
            value_2_chunk_offset++;
            if (unlikely(value_2_chunk_index == -1 || value_2_chunk_offset >= value_2_chunk_info->chunk_length)) {
                if (unlikely(value_2_chunk_data_allocated_new)) {
                    xalloc_free(value_2_chunk_data);
                    value_2_chunk_data_allocated_new = false;
                }

                value_2_chunk_index++;
                value_2_chunk_info = storage_db_chunk_sequence_get(value_2, value_2_chunk_index);
                value_2_chunk_data = storage_db_get_chunk_data(
                        db,
                        value_2_chunk_info,
                        &value_2_chunk_data_allocated_new);

                if (value_2_chunk_index == 0) {
                    value_2_chunk_offset = -1;
                } else {
                    value_2_chunk_offset = 0;
                }
            }

            uint32_t value;
            if (value_1_char_index == 0 || value_2_char_index == 0) {
                value = 0;
            } else {
                if (value_1_chunk_data[value_1_chunk_offset] == value_2_chunk_data[value_2_chunk_offset]) {
                    value = lcsmap_get(
                            lcsmap,
                            value_1->size,
                            value_1_char_index - 1,
                            value_2_char_index - 1) + 1;
                } else {
                    uint32_t lcs1 = lcsmap_get(
                            lcsmap,
                            value_1->size,
                            value_1_char_index - 1,
                            value_2_char_index);
                    uint32_t lcs2 = lcsmap_get(
                            lcsmap,
                            value_1->size,
                            value_1_char_index,
                            value_2_char_index - 1);
                    value = lcs1 > lcs2 ? lcs1 : lcs2;
                }
            }

            lcsmap_set(
                    lcsmap,
                    value_1->size,
                    value_1_char_index,
                    value_2_char_index,
                    value);
        }
    }

    // Cleanup after building the tree
    if (unlikely(value_1_chunk_data_allocated_new)) {
        xalloc_free(value_1_chunk_data);
        value_1_chunk_data_allocated_new = false;
    }
    value_1_chunk_data = NULL;
    value_1_chunk_info = NULL;

    if (unlikely(value_2_chunk_data_allocated_new)) {
        xalloc_free(value_2_chunk_data);
        value_2_chunk_data_allocated_new = false;
    }
    value_2_chunk_data = NULL;
    value_2_chunk_info = NULL;

    return lcsmap;
}

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(lcs) {
    bool return_res = true;
    char *lcs_string = NULL;
    uint32_t *lcsmap = NULL;
    uint32_t lcs_string_length = 0;
    storage_db_entry_index_t *entry_index_1 = NULL, *entry_index_2 = NULL;
    bool value_1_chunk_data_allocated_new = false, value_2_chunk_data_allocated_new = false;
    char *value_1_chunk_data = NULL, *value_2_chunk_data = NULL;
    module_redis_command_lcs_context_t *context = connection_context->command.context;

    transaction_t transaction = { 0 };
    transaction_acquire(&transaction);

    if (context->idx_idx.has_token || context->minmatchlen_len.has_token || context->withmatchlen_withmatchlen.has_token) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR the IDX, MINMATCHLEN and WITHMATCHLEN parameters are not yet supported");
        goto end;
    }

    if (context->idx_idx.has_token && context->len_len.has_token) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR If you want both the length and indexes, please just use IDX.");
        goto end;
    }

    if (!(entry_index_1 = storage_db_get_entry_index_for_read(
            connection_context->db,
            connection_context->database_number,
            &transaction,
            context->key1.value.key,
            context->key1.value.length))) {
        goto end;
    }

    if (!(entry_index_2 = storage_db_get_entry_index_for_read(
            connection_context->db,
            connection_context->database_number,
            &transaction,
            context->key2.value.key,
            context->key2.value.length))) {
        goto end;
    }

    if (entry_index_1->value.size == 0 || entry_index_2->value.size == 0) {
        goto end;
    }

    // Fetch the chunk sequences
    storage_db_chunk_sequence_t *value_1 = &entry_index_1->value;
    storage_db_chunk_sequence_t *value_2 = &entry_index_2->value;

    // Build the lcs map
    lcsmap = lcsmap_build(connection_context->db, value_1, value_2);

    // Get the length of the longest substring
    lcs_string_length = lcsmap_get(
            lcsmap,
            value_1->size,
            value_1->size,
            value_2->size);

    uint32_t value_1_offset_plus_one = value_1->size, value_2_offset_plus_one = value_2->size;

    if (!context->len_len.has_token) {
        lcs_string = xalloc_alloc_zero(lcs_string_length + 1);
    }

    value_1_chunk_data_allocated_new = false;
    value_1_chunk_data = storage_db_get_chunk_data(
            connection_context->db,
            storage_db_chunk_sequence_get(value_1, 0),
            &value_1_chunk_data_allocated_new);
    if (unlikely(value_1_chunk_data == NULL)) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR lcs failed");
        goto end;
    }

    value_2_chunk_data_allocated_new = false;
    value_2_chunk_data = storage_db_get_chunk_data(
            connection_context->db,
            storage_db_chunk_sequence_get(value_2, 0),
            &value_2_chunk_data_allocated_new);
    if (unlikely(value_2_chunk_data == NULL)) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR lcs failed");
        goto end;
    }

    uint32_t lcs_string_index = lcs_string_length;
    while (value_1_offset_plus_one > 0 && value_2_offset_plus_one > 0) {
        if (value_1_chunk_data[value_1_offset_plus_one - 1] == value_2_chunk_data[value_2_offset_plus_one - 1]) {
            if (!context->len_len.has_token) {
                lcs_string[lcs_string_index - 1] = value_1_chunk_data[value_1_offset_plus_one - 1];
            }

            lcs_string_index--;
            value_1_offset_plus_one--;
            value_2_offset_plus_one--;
        } else {
            uint32_t lcs1 = lcsmap_get(
                    lcsmap,
                    value_1->size,
                    value_1_offset_plus_one - 1,
                    value_2_offset_plus_one);
            uint32_t lcs2 = lcsmap_get(
                    lcsmap,
                    value_1->size,
                    value_1_offset_plus_one,
                    value_2_offset_plus_one - 1);

            if (lcs1 > lcs2) {
                value_1_offset_plus_one--;
            } else {
                value_2_offset_plus_one--;
            }
        }
    }

end:

    if (likely(!module_redis_connection_has_error(connection_context))) {
        if (!context->len_len.has_token && !context->idx_idx.has_token) {
            if (unlikely(lcs_string == NULL)) {
                return_res = module_redis_connection_send_blob_string(
                        connection_context,
                        "",
                        0);
            } else {
                return_res = module_redis_connection_send_blob_string(
                        connection_context,
                        lcs_string,
                        strlen(lcs_string));
            }
        } else if (context->len_len.has_token) {
            return_res = module_redis_connection_send_number(
                    connection_context,
                    lcs_string_length);
        } else {
            assert(false);
        }
    }

    transaction_release(&transaction);

    if (lcsmap != NULL) {
        xalloc_free(lcsmap);
    }

    if (value_1_chunk_data_allocated_new) {
        xalloc_free(value_2_chunk_data);
    }

    if (value_2_chunk_data_allocated_new) {
        xalloc_free(value_2_chunk_data);
    }

    if (entry_index_1) {
        storage_db_entry_index_status_decrease_readers_counter(entry_index_1, NULL);
        entry_index_1 = NULL;
    }

    if (entry_index_2) {
        storage_db_entry_index_status_decrease_readers_counter(entry_index_2, NULL);
        entry_index_2 = NULL;
    }

    return return_res;
}
