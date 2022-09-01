/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <math.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "clock.h"
#include "spinlock.h"
#include "utils_string.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "slab_allocator.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"
#include "module/redis/module_redis_connection.h"

#define TAG "module_redis_command_helper_incr_decr"

bool module_redis_command_helper_incr_decr(
        module_redis_connection_context_t *connection_context,
        int64_t amount,
        char **key,
        size_t *key_length) {
    char *current_string = NULL;
    bool return_res = false;
    bool abort_rmw = true;
    bool allocated_new_buffer = false;
    storage_db_op_rmw_status_t rmw_status = { 0 };
    storage_db_entry_index_t *current_entry_index = NULL;
    storage_db_chunk_sequence_t *chunk_sequence_new = NULL;

    int64_t number = 0, new_number;
    storage_db_expiry_time_ms_t expiry_time_ms = STORAGE_DB_ENTRY_NO_EXPIRY;

    if (unlikely(!storage_db_op_rmw_begin(
            connection_context->db,
            *key,
            *key_length,
            &rmw_status,
            &current_entry_index))) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR operation failed");

        goto end;
    }

    if (likely(current_entry_index)) {
        expiry_time_ms = current_entry_index->expiry_time_ms;

        storage_db_chunk_info_t *chunk_info = storage_db_chunk_sequence_get(
                current_entry_index->value,
                0);
        current_string = storage_db_get_chunk_data(
                connection_context->db,
                chunk_info,
                &allocated_new_buffer);
        if (unlikely(current_string == NULL)) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR operation failed");
            goto end;
        }

        bool invalid = false;
        number = utils_string_to_int64(current_string, chunk_info->chunk_length, &invalid);
        if (unlikely(invalid)) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR value is not an integer or out of range");

            goto end;
        }
    }

    new_number = number + amount;
    bool overflow = (amount > 0 && new_number < number) || (amount < 0 && new_number > number);

    if (unlikely(overflow)) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR increment or decrement would overflow");

        goto end;
    }

    // A static buffer is just fine as the maximum number of chars of an int64 with sign is 20
    char buffer[32] = { 0 };
    size_t buffer_length = snprintf(buffer, sizeof(buffer), "%ld", new_number);

    chunk_sequence_new = storage_db_chunk_sequence_allocate(connection_context->db, buffer_length);
    if (unlikely(!storage_db_chunk_write(
            connection_context->db,
            storage_db_chunk_sequence_get(chunk_sequence_new, 0),
            0,
            buffer,
            buffer_length))) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR operation failed");

        goto end;
    }

    if (!storage_db_op_rmw_commit_update(
            connection_context->db,
            &rmw_status,
            chunk_sequence_new,
            expiry_time_ms)) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR operation failed");

        goto end;
    }

    *key = NULL;
    *key_length = 0;
    chunk_sequence_new = NULL;
    abort_rmw = false;

    if ((return_res = module_redis_connection_send_number(
            connection_context,
            new_number)) == false) {
        goto end;
    }

    return_res = true;

end:

    if (allocated_new_buffer) {
        slab_allocator_mem_free(current_string);
    }

    if (unlikely(abort_rmw)) {
        storage_db_op_rmw_abort(connection_context->db, &rmw_status);
    }

    if (unlikely(chunk_sequence_new)) {
        storage_db_chunk_sequence_free(connection_context->db, chunk_sequence_new);
    }

    return return_res;
}

bool module_redis_command_helper_incr_decr_float(
        module_redis_connection_context_t *connection_context,
        long double amount,
        char **key,
        size_t *key_length) {
    size_t new_number_buffer_length;
    char new_number_buffer_static[128] = { 0 };
    char *new_number_buffer = NULL;
    bool new_number_allocated_buffer = false;
    char *current_string = NULL;
    bool return_res = false;
    bool abort_rmw = true;
    bool allocated_new_buffer = false;
    storage_db_op_rmw_status_t rmw_status = { 0 };
    storage_db_entry_index_t *current_entry_index = NULL;
    storage_db_chunk_sequence_t *chunk_sequence_new = NULL;

    long double number = 0, new_number;
    storage_db_expiry_time_ms_t expiry_time_ms = STORAGE_DB_ENTRY_NO_EXPIRY;

    if (unlikely(!storage_db_op_rmw_begin(
            connection_context->db,
            *key,
            *key_length,
            &rmw_status,
            &current_entry_index))) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR op failed");

        goto end;
    }

    if (likely(current_entry_index)) {
        expiry_time_ms = current_entry_index->expiry_time_ms;

        storage_db_chunk_info_t *chunk_info = storage_db_chunk_sequence_get(
                current_entry_index->value,
                0);
        current_string = storage_db_get_chunk_data(
                connection_context->db,
                chunk_info,
                &allocated_new_buffer);
        if (unlikely(current_string == NULL)) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR operation failed");
            goto end;
        }

        bool invalid = false;
        number = utils_string_to_long_double(current_string, chunk_info->chunk_length, &invalid);
        if (unlikely(invalid)) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR value is not a valid float");

            goto end;
        }
    }

    new_number = number + amount;

    if (isnan(new_number) || isinf(new_number)) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR increment would produce NaN or Infinity");

        goto end;
    }

    // Doubles can be long almost 5kb (as per DBL_MAX and DBL_MIN) so allocate 6 * 1024 using a static buffer

    // For Redis compatibility
    if ((int64_t)new_number == new_number) {
        new_number_buffer = (char*)new_number_buffer_static;
        new_number_buffer_length = snprintf(new_number_buffer, sizeof(new_number_buffer_static), "%ld", (int64_t)new_number);
    } else {
        bool new_number_has_trailing_zeros = false;

        new_number_buffer_length = snprintf(NULL, 0, "%.17Lf", new_number);
        if (unlikely(new_number_buffer_length > sizeof(new_number_buffer_static))) {
            new_number_buffer = slab_allocator_mem_alloc(new_number_buffer_length + 1);
            new_number_allocated_buffer = true;
        } else {
            new_number_buffer = (char*)new_number_buffer_static;
        }

        // %Lf always produces trailing zeros and for compatibility with Redis we want to drop them
        new_number_buffer_length = snprintf(new_number_buffer, new_number_buffer_length + 1, "%.17Lf", new_number);

        // Fast path for the numbers starting with 0.
        if (likely(new_number_buffer_length >= 2 && new_number_buffer[1] == '.')) {
            new_number_has_trailing_zeros = true;
        } else if (new_number_buffer_length > 1) {
            for(int index = 1; likely(index < new_number_buffer_length); index++) {
                if (unlikely(new_number_buffer[index] == '.')) {
                    new_number_has_trailing_zeros = true;
                    break;
                }
            }
        }

        // It's likely the number has trailing zeros
        if (likely(new_number_has_trailing_zeros)) {
            char *new_number_buffer_ptr = new_number_buffer + new_number_buffer_length - 1;
            while(*new_number_buffer_ptr == '0') {
                new_number_buffer_ptr--;
                new_number_buffer_length--;
            }

            // If only the dot is at the end after removing the trailing zeros drop it
            if (*new_number_buffer_ptr == '.') {
                new_number_buffer_length--;
            }

            // If the resulting string formatting is -0, convert it just to zero
            if (new_number_buffer_length == 2 && new_number_buffer[0] == '-' && new_number_buffer[1] == '0') {
                new_number_buffer[0] = '0';
                new_number_buffer_length = 1;
            }
        }
    }

    new_number_buffer[new_number_buffer_length] = 0;

    chunk_sequence_new = storage_db_chunk_sequence_allocate(connection_context->db, new_number_buffer_length);
    if (unlikely(!storage_db_chunk_write(
            connection_context->db,
            storage_db_chunk_sequence_get(chunk_sequence_new, 0),
            0,
            new_number_buffer,
            new_number_buffer_length))) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR operation failed");

        goto end;
    }

    if (!storage_db_op_rmw_commit_update(
            connection_context->db,
            &rmw_status,
            chunk_sequence_new,
            expiry_time_ms)) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR operation failed");

        goto end;
    }

    *key = NULL;
    *key_length = 0;
    chunk_sequence_new = NULL;
    abort_rmw = false;

    if ((return_res = module_redis_connection_send_string(
            connection_context,
            new_number_buffer,
            new_number_buffer_length)) == false) {
        goto end;
    }

    return_res = true;

end:

    if (new_number_allocated_buffer) {
        slab_allocator_mem_free(new_number_buffer);
    }

    if (allocated_new_buffer) {
        slab_allocator_mem_free(current_string);
    }

    if (unlikely(abort_rmw)) {
        storage_db_op_rmw_abort(connection_context->db, &rmw_status);
    }

    if (unlikely(chunk_sequence_new)) {
        storage_db_chunk_sequence_free(connection_context->db, chunk_sequence_new);
    }

    return return_res;
}
