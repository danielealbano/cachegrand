/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <fcntl.h>
#include <string.h>
#include <liblzf/lzf.h>
#include <stdlib.h>

#include "exttypes.h"
#include "misc.h"
#include "clock.h"
#include "fiber/fiber.h"
#include "log/log.h"
#include "xalloc.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "memory_allocator/ffma.h"
#include "config.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "storage/storage.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "fiber/fiber_scheduler.h"
#include "worker/worker_op.h"
#include "module/redis/snapshot/module_redis_snapshot.h"

#include "module_redis_snapshot_load.h"

#define TAG "module_redis_snapshot_load"

static off_t rdb_offset = 0;
static uint64_t rdb_checksum = 0;
static uint64_t counter_strings = 0;
static uint64_t counter_expires = 0;
static uint64_t counter_expires_expired = 0;
static uint64_t rdb_load_start;

size_t module_redis_snapshot_load_read(
        storage_channel_t *channel,
        void *buffer,
        size_t length) {
    if (!storage_read(channel, buffer, length, rdb_offset)) {
        FATAL(TAG, "Unable to read <%lu> bytes at offset <%lu> from storage channel", length, rdb_offset);
    }

    rdb_offset += (off_t)length;

    return length;
}

uint64_t module_redis_snapshot_load_read_length_encoded_int(
        storage_channel_t *channel) {
    uint8_t byte;
    uint64_t length = 0;
    module_redis_snapshot_load_read(channel, (char *) &byte, 1);

    if ((byte & 0xC0) == 0) {
        length = byte & 0x3F;
    } else if ((byte & 0xC0) == 0x40) {
        uint8_t next_byte;
        module_redis_snapshot_load_read(channel, (char *) &next_byte, 1);
        length = ((byte & 0x3F) << 8) | next_byte;
    } else if ((byte & 0xC0) == 0x80 && (byte & 0x01) == 0) {
        uint32_t length32;
        module_redis_snapshot_load_read(channel, (char *) &length32, 4);
        length = int32_ntoh(length32);
    } else if ((byte & 0xC0) == 0x80 && (byte & 0x01) == 1) {
        module_redis_snapshot_load_read(channel, (char *) &length, 8);
        length = int64_ntoh(length);
    }

    return length;
}

void *module_redis_snapshot_load_read_string(
        storage_channel_t *channel,
        size_t *length) {
    uint8_t byte;
    module_redis_snapshot_load_read(channel, &byte, 1);

    if ((byte & 0xC0) == 0) {
        *length = byte & 0x3F;
    } else if ((byte & 0xC0) == 0x40) {
        uint8_t next_byte;
        module_redis_snapshot_load_read(channel, &next_byte, 1);
        *length = ((byte & 0x3F) << 8) | next_byte;
    } else if ((byte & 0xC0) == 0x80 && (byte & 0x01) == 0) {
        uint32_t len;
        module_redis_snapshot_load_read(channel, (char *) &len, 4);
        len = int32_ntoh(len);
        *length = len;
    } else if ((byte & 0xC0) == 0x80 && (byte & 0x01) == 1) {
        uint64_t len;
        module_redis_snapshot_load_read(channel, (char *) &len, 8);
        len = int64_ntoh(len);
        *length = len;
    } else {
        uint8_t type = byte & 0x3F;

        switch (type) {
            case 0: { // 8-bit integer
                int8_t int8_value;
                module_redis_snapshot_load_read(channel, &int8_value, 1);
                *length = snprintf(NULL, 0, "%d", int8_value);
                char *buf = xalloc_alloc(*length + 1);
                snprintf(buf, (*length) + 1, "%d", int8_value);
                return buf;
            }
            case 1: { // 16-bit integer
                int16_t int16_value;
                module_redis_snapshot_load_read(channel, &int16_value, 2);
                *length = snprintf(NULL, 0, "%d", int16_value);
                char *buf = xalloc_alloc(*length + 1);
                snprintf(buf, (*length) + 1, "%d", int16_value);
                return buf;
            }
            case 2: { // 32-bit integer
                int32_t int32_value;
                module_redis_snapshot_load_read(channel, &int32_value, 4);
                *length = snprintf(NULL, 0, "%d", int32_value);
                char *buf = xalloc_alloc(*length + 1);
                snprintf(buf, (*length) + 1, "%d", int32_value);
                return buf;
            }
            case 3: { // LZF compressed string
                uint64_t compressed_length = module_redis_snapshot_load_read_length_encoded_int(channel);
                uint64_t uncompressed_length = module_redis_snapshot_load_read_length_encoded_int(channel);
                void *compressed_buf = xalloc_alloc(compressed_length);
                module_redis_snapshot_load_read(channel, compressed_buf, compressed_length);
                void *uncompressed_buf = xalloc_alloc(uncompressed_length);
                if (lzf_decompress(
                        compressed_buf,
                        compressed_length,
                        uncompressed_buf,
                        uncompressed_length) == 0) {
                    xalloc_free(compressed_buf);
                    xalloc_free(uncompressed_buf);
                    FATAL(TAG, "Unable to decompress LZF compressed string");
                }
                xalloc_free(compressed_buf);
                *length = uncompressed_length;
                return uncompressed_buf;
            }
            default: {
                FATAL(TAG, "Unknown string encoding");
            }
        }
    }

    void *buf = xalloc_alloc(*length);
    module_redis_snapshot_load_read(channel, buf, *length);
    return buf;
}

bool module_redis_snapshot_load_validate_magic(
        storage_channel_t *channel) {
    char magic[5] = { 0 };
    module_redis_snapshot_load_read(channel, magic, 5);

    if (strncmp(magic, "REDIS", 5) != 0) {
        return false;
    }

    return true;
}

bool module_redis_snapshot_load_validate_version(
        storage_channel_t *channel) {
    char *endptr = NULL;
    char version[5] = { 0 };

    // Read only 4 bytes with a buffer of 5 to ensure that it's always null terminated as we need to use atoi
    module_redis_snapshot_load_read(channel, version, 4);

    // Check that version starts with 2 zeros
    if (strncmp(version, "00", 2) != 0) {
        return false;
    }

    // Check that the version is less or equal than 11
    if (strtol(version, &endptr, 10) > 11) {
        return false;
    }

    // Validate that the version ends with a null byte
    if (endptr == NULL || *endptr != '\0') {
        return false;
    }

    return true;
}

bool module_redis_snapshot_load_validate_checksum(
        storage_channel_t *channel) {
    uint64_t checksum;
    module_redis_snapshot_load_read(channel, &checksum, 8);
    checksum = int64_ntoh(checksum);

    if (checksum == 0) {
        return true;
    }

    // TODO: validate checksum

    return true;
}

uint8_t module_redis_snapshot_load_read_opcode(
        storage_channel_t *channel) {
    uint8_t opcode;
    module_redis_snapshot_load_read(channel, &opcode, 1);
    return opcode;
}

void module_redis_snapshot_load_process_opcode_aux(
        storage_channel_t *channel) {
    size_t key_length;
    size_t value_length;
    void *key = module_redis_snapshot_load_read_string(channel, &key_length);
    void *value = module_redis_snapshot_load_read_string(channel, &value_length);

    LOG_V(
            TAG,
            "RDB Auxiliary field: key = %.*s, value = %.*s",
            (int)key_length,
            (char *)key,
            (int)value_length,
            (char *)value);

    xalloc_free(key);
    xalloc_free(value);
}

void module_redis_snapshot_load_process_opcode_db_number(
        storage_channel_t *channel) {
    uint32_t db_number = module_redis_snapshot_load_read_length_encoded_int(channel);
    if (db_number != 0) {
        FATAL(TAG, "Unsupported DB number: %d", db_number);
    }

    LOG_V(TAG, "RDB DB number: %d", db_number);
}

void module_redis_snapshot_load_process_opcode_resize_db(
        storage_channel_t *channel) {
    uint64_t db_size = module_redis_snapshot_load_read_length_encoded_int(channel);
    uint64_t expires_size = module_redis_snapshot_load_read_length_encoded_int(channel);

    LOG_V(TAG, "RDB DB size: %lu", db_size);
    LOG_V(TAG, "RDB DB expires size: %lu", expires_size);
}

uint64_t module_redis_snapshot_load_process_opcode_expire_time(
        storage_channel_t *channel,
        uint8_t opcode) {
    uint64_t expiry_ms = 0;

    if (opcode == MODULE_REDIS_SNAPSHOT_OPCODE_EXPIRE_TIME_MS) {
        module_redis_snapshot_load_read(channel, &expiry_ms, 8);
    } else if (opcode == MODULE_REDIS_SNAPSHOT_OPCODE_EXPIRE_TIME) {
        uint32_t expiry_s = 0;
        module_redis_snapshot_load_read(channel, &expiry_s, 4);
        expiry_ms = expiry_s * 1000;
    }

    counter_expires++;
    if (expiry_ms <= rdb_load_start) {
        counter_expires_expired++;
    }

    return expiry_ms;
}

bool module_redis_snapshot_load_write_key_value_string_chunk_sequence(
        storage_db_t *db,
        char* string,
        size_t string_length,
        storage_db_chunk_sequence_t *chunk_sequence) {
    size_t written_data = 0;
    storage_db_chunk_index_t current_chunk_index = 0;
    do {
        storage_db_chunk_info_t *chunk_info = storage_db_chunk_sequence_get(chunk_sequence, current_chunk_index);

        size_t chunk_length_to_write = string_length - written_data;
        size_t chunk_data_to_write_length =
                chunk_length_to_write > chunk_info->chunk_length ? chunk_info->chunk_length : chunk_length_to_write;

        bool res = storage_db_chunk_write(
                db,
                chunk_info,
                0,
                string + written_data,
                chunk_data_to_write_length);

        if (!res) {
            LOG_E(
                    TAG,
                    "Unable to write value chunk <%u> long <%u> bytes",
                    current_chunk_index,
                    chunk_info->chunk_length);
            return false;
        }

        written_data += chunk_data_to_write_length;
        current_chunk_index++;
    } while(written_data < string_length);

    return true;
}

bool module_redis_snapshot_load_write_key_value_string(
        storage_db_t *db,
        char* key,
        size_t key_length,
        char* value,
        size_t value_length,
        uint64_t expiry_ms) {
    storage_db_chunk_sequence_t chunk_sequence;

    if (unlikely(!storage_db_chunk_sequence_allocate(db, &chunk_sequence, value_length))) {
        return false;
    }

    if (unlikely(!module_redis_snapshot_load_write_key_value_string_chunk_sequence(db, value, value_length, &chunk_sequence))) {
        return false;
    }

    if (unlikely(!storage_db_op_set(
            db,
            key,
            key_length,
            STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_STRING,
            &chunk_sequence,
            expiry_ms))) {
        return false;
    }

    return true;
}

void module_redis_snapshot_load_process_value_string(
        storage_channel_t *channel,
        uint64_t expiry_ms) {
    bool set_failed = false;
    size_t key_length;
    size_t value_length;
    char *key, *value;

    key = module_redis_snapshot_load_read_string(channel, &key_length);
    value = module_redis_snapshot_load_read_string(channel, &value_length);

    counter_strings++;

    if (likely(expiry_ms == 0 || expiry_ms > rdb_load_start)) {
        storage_db_t *db = worker_context_get()->db;

        if (!module_redis_snapshot_load_write_key_value_string(db, key, key_length, value, value_length, expiry_ms)) {
            set_failed = true;
            goto end;
        }
    } else {
        LOG_V(TAG, "> Skipping expired key-value pair");
    }

    end:

    if (set_failed) {
        xalloc_free(key);
        xalloc_free(value);
        FATAL(TAG, "Unable to set key-value pair");
    }
}

void module_redis_snapshot_load_data(
        storage_channel_t *channel) {
    uint8_t opcode;
    uint64_t expiry_ms = 0;

    // Validate the magic string
    if (module_redis_snapshot_load_validate_magic(channel) == false) {
        FATAL(TAG, "Invalid magic string");
    }

    // Validate the version
    if (module_redis_snapshot_load_validate_version(channel) == false) {
        FATAL(TAG, "Invalid or unsupported version");
    }

    // Process the opcodes
    while ((opcode = module_redis_snapshot_load_read_opcode(channel)) != MODULE_REDIS_SNAPSHOT_OPCODE_EOF) {
        switch (opcode) {
            case MODULE_REDIS_SNAPSHOT_OPCODE_AUX:
                module_redis_snapshot_load_process_opcode_aux(channel);
                break;

            case MODULE_REDIS_SNAPSHOT_OPCODE_DB_NUMBER:
                module_redis_snapshot_load_process_opcode_db_number(channel);
                break;

            case MODULE_REDIS_SNAPSHOT_OPCODE_RESIZE_DB:
                module_redis_snapshot_load_process_opcode_resize_db(channel);
                break;

            case MODULE_REDIS_SNAPSHOT_OPCODE_EXPIRE_TIME:
            case MODULE_REDIS_SNAPSHOT_OPCODE_EXPIRE_TIME_MS:
                expiry_ms = module_redis_snapshot_load_process_opcode_expire_time(channel, opcode);
                break;

            case MODULE_REDIS_SNAPSHOT_VALUE_TYPE_STRING:
                module_redis_snapshot_load_process_value_string(channel, expiry_ms);
                break;

            default:
                FATAL(TAG, "Unknown opcode or value type <0x%X> at offset <%lu>", opcode, rdb_offset - 1);
        }

        // If it's not an opcode than it's a value type and therefore the expiry can be reset
        if (opcode < MODULE_REDIS_SNAPSHOT_OPCODES_MIN) {
            expiry_ms = 0;
        }
    }

    if (!module_redis_snapshot_load_validate_checksum(channel)) {
        FATAL(TAG, "Invalid checksum");
    }
}

bool module_redis_snapshot_load_check_file_exists(
        char *path) {
    // Check if the snapshot file exists
    if (access(path, F_OK) == -1) {
        return false;
    }

    return true;
}

bool module_redis_snapshot_load(
        char *path) {
    // Check if the snapshot file exists
    if (!module_redis_snapshot_load_check_file_exists(path)) {
        // If the file don't exist just return, it's not an error
        LOG_I(TAG, "Snapshot file <%s> does not exist", path);
        return true;
    }

    // Open the snapshot file
    char *snapshot_path = ffma_mem_alloc(strlen(path) + 1);
    strcpy(snapshot_path, path);
    storage_channel_t *snapshot_channel = storage_open(
            snapshot_path,
            0,
            O_RDONLY);
    if (!snapshot_channel) {
        LOG_E(TAG, "Unable to open the snapshot file <%s>", path);
        return false;
    }

    // Reset the internal variables
    rdb_offset = 0;
    rdb_load_start = clock_realtime_int64_ms();
    rdb_checksum = 0;
    counter_strings = 0;
    counter_expires = 0;
    counter_expires_expired = 0;

    // Load the data
    module_redis_snapshot_load_data(snapshot_channel);

    // Close the snapshot file
    storage_close(snapshot_channel);

    // Report the results
    LOG_I(TAG, "Loaded the snapshot file <%s>", path);
    LOG_I(TAG, "Found:");
    LOG_I(TAG, "> %lu strings", counter_strings);
    LOG_I(TAG, "> %lu values with expirations", counter_expires - counter_expires_expired);
    LOG_I(TAG, "> %lu values expired", counter_expires_expired);

    return true;
}
