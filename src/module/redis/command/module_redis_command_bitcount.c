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
#include <stdarg.h>
#include <string.h>
#include <arpa/inet.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "memory_allocator/ffma.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"
#include "module/redis/module_redis_connection.h"

#define TAG "module_redis_command_bitcount"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(bitcount) {
    storage_db_entry_index_t *entry_index = NULL;
    storage_db_chunk_index_t chunk_index = 0;
    storage_db_chunk_info_t *chunk_info = NULL;
    uint64_t chunk_data_offset = 0;
    int64_t bit_count = 0;
    bool allocated_new_buffer = false;
    char *buffer_to_process = NULL;
    bool range_is_byte = true;
    bool return_res = true;
    uint64_t offset = 0;
    uint64_t start_byte = 0, end_byte = 0, end_byte_loop = 0;
    uint8_t start_mask = 0, end_mask = 0;
    bool has_start_mask = false, has_end_mask = false;
    module_redis_command_bitcount_context_t *context = connection_context->command.context;

    if (context->unit.value.unit_bit.has_token) {
        range_is_byte = false;
    }

    // Fetch the entry
    entry_index = storage_db_get_entry_index_for_read(
            connection_context->db,
            connection_context->database_number,
            context->key.value.key,
            context->key.value.length);

    if (unlikely(!entry_index)) {
        return_res = module_redis_connection_send_number(connection_context, 0);
        goto end;
    }

    if (context->range.value.start.value != 0 || context->range.value.end.value != 0) {
        uint64_t start, end, length_in_unit;

        // Acquire the start and the end passed by the command
        start = context->range.value.start.value;
        end = context->range.value.end.value;

        // Acquire the length of the data and convert them in bit if necessary
        length_in_unit = entry_index->value.size;
        if (!range_is_byte) {
            length_in_unit *= 8;
        }

        // Calculate start and end
        if (context->range.value.start.value < 0) {
            start = length_in_unit + start;
        } else {
            start = context->range.value.start.value;
        }

        if (context->range.value.end.value < 0) {
            end = length_in_unit + end;
        } else {
            end = context->range.value.end.value;
        }

        // Calculate the start and the end in bytes and, if necessary, the masks for the first and last byte
        if (range_is_byte) {
            start_byte = start;
            end_byte = end;
        } else {
            start_byte = start / 8;
            end_byte = end / 8;

            start_mask = 0xFF >> (start % 8);
            has_start_mask = start_mask != 0xFF;

            if (start_byte != end_byte) {
                end_mask = (uint8_t)(0xFF << (8 - ((end+1) % 8)));
                has_end_mask = end_mask != 0xFF;
            }
        }

        if (start > end || start_byte >= entry_index->value.size || end_byte >= entry_index->value.size) {
            return_res = module_redis_connection_send_number(connection_context, 0);
            goto end;
        }
    } else {
        start_byte = 0;
        end_byte = entry_index->value.size;
    }

    // Acquire the start as offset
    offset = start_byte;

    // Skip the chunks until it reaches one containing range_start
    uint64_t temp_search_offset = offset;
    for (; chunk_index < entry_index->value.count; chunk_index++) {
        chunk_info = storage_db_chunk_sequence_get(&entry_index->value, chunk_index);

        if (temp_search_offset < chunk_info->chunk_length) {
            break;
        }

        temp_search_offset -= chunk_info->chunk_length;
    }

    // The remaining offset is from where to start in the chunk
    chunk_data_offset = temp_search_offset;

    // Load the initial chuck
    chunk_info = storage_db_chunk_sequence_get(&entry_index->value, chunk_index);

    // Always load the first chunk to simply the logic that follows
    if (unlikely((buffer_to_process = storage_db_get_chunk_data(
            connection_context->db,
            chunk_info,
            &allocated_new_buffer)) == NULL)) {
        return_res = false;
        goto end;
    }

    // Special case if there is a mask set for the start
    if (has_start_mask) {
        uint8_t byte = *(uint8_t*)(buffer_to_process + chunk_data_offset);
        byte &= start_mask;
        bit_count += __builtin_popcount(byte);
        chunk_data_offset++;
        offset++;
    }

    // Calculate the end byte for the loop
    end_byte_loop = end_byte;
    if (has_end_mask) {
        end_byte_loop--;
    }

    // Process the chunks
    while (offset <= end_byte_loop) {
        // Process the chunk 4 bytes at the time with __builtin_popcountll
        for (
                ;
                chunk_data_offset + 4 < chunk_info->chunk_length && offset + 4 <= end_byte_loop;
                (offset += 4) && (chunk_data_offset += 4)) {
            uint32_t *data = (uint32_t*)(buffer_to_process + chunk_data_offset);
            bit_count += __builtin_popcountll(*data);
        }

        // Process the chunk 2 bytes at the time with __builtin_popcountl
        for (
                ;
                chunk_data_offset + 2 < chunk_info->chunk_length && offset + 2 <= end_byte_loop;
                (offset += 2) && (chunk_data_offset += 2)) {
            uint16_t *data = (uint16_t*)(buffer_to_process + chunk_data_offset);
            bit_count += __builtin_popcountl(*data);
        }

        // Process the chunk byte by byte with __builtin_popcount
        for (
                ;
                chunk_data_offset < chunk_info->chunk_length && offset <= end_byte_loop;
                (offset++) && (chunk_data_offset++)) {
            uint8_t byte = *(uint8_t*)(buffer_to_process + chunk_data_offset);
            bit_count += __builtin_popcount(byte);
        }

        // Increment the offset
        offset += chunk_info->chunk_length - chunk_data_offset;

        // Check if the end of the range has been reached
        if (offset >= end_byte_loop || chunk_index == entry_index->value.count - 1) {
            break;
        }

        // Reset the offset for the next chunk, don't reset if the loop is interrupted early as it might be necessary
        // to access the current chunk again to process the end mask
        chunk_data_offset = 0;

        // Free up the memory
        if (allocated_new_buffer) {
            ffma_mem_free(buffer_to_process);
            allocated_new_buffer = false;
        }

        // Load the next chunk
        chunk_info = storage_db_chunk_sequence_get(&entry_index->value, chunk_index);
        if (unlikely((buffer_to_process = storage_db_get_chunk_data(
                connection_context->db,
                chunk_info,
                &allocated_new_buffer)) == NULL)) {
            return_res = false;
            goto end;
        }
    }

    if (has_end_mask) {
        // Check if the chunk has been read all, and in case load a new one if available
        if (chunk_data_offset >= chunk_info->chunk_length) {
            // Free up the memory
            if (allocated_new_buffer) {
                ffma_mem_free(buffer_to_process);
                allocated_new_buffer = false;
            }

            chunk_index++;
            chunk_info = storage_db_chunk_sequence_get(&entry_index->value, chunk_index);
            if (unlikely((buffer_to_process = storage_db_get_chunk_data(
                    connection_context->db,
                    chunk_info,
                    &allocated_new_buffer)) == NULL)) {
                return_res = false;
                goto end;
            }

            chunk_data_offset = 0;
        }

        uint8_t byte = *(uint8_t*)(buffer_to_process + chunk_data_offset);
        byte &= end_mask;
        bit_count += __builtin_popcount(byte);
    }

    return_res = module_redis_connection_send_number(connection_context, bit_count);

end:
    // Free up the memory
    if (allocated_new_buffer) {
        ffma_mem_free(buffer_to_process);
        allocated_new_buffer = false;
    }

    return return_res;
}
