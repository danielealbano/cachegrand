///**
// * Copyright (C) 2020-2021 Daniele Salvatore Albano
// * All rights reserved.
// *
// * This software may be modified and distributed under the terms
// * of the BSD license.  See the LICENSE file for details.
// **/
//
//#define _GNU_SOURCE
//
//#include <stdlib.h>
//#include <stdint.h>
//#include <stdbool.h>
//#include <math.h>
//#include <clock.h>
//#include <assert.h>
//#include <linux/limits.h>
//#include <string.h>
//#include <stdio.h>
//#include <fcntl.h>
//
//#include "misc.h"
//#include "exttypes.h"
//#include "xalloc.h"
//#include "log/log.h"
//#include "spinlock.h"
//#include "data_structures/double_linked_list/double_linked_list.h"
//#include "slab_allocator.h"
//#include "data_structures/double_linked_list/double_linked_list.h"
//#include "storage/io/storage_io_common.h"
//#include "storage/channel/storage_channel.h"
//#include "storage/storage.h"
//
//#include "storage_kvdb.h"
//
//#define TAG "storage_kvdb"
//
//// TODO: missing primitive to opendir, if a dir is not opened and flushed a crash may lead to metadata in buffers not
////       being written defacto making unreadable the data
//// TODO: new entries that are too small to trigger a write must be placed into the hot cache of the hashtable value
////       and marked as to_be_written
//// TODO: when the buffer is going to be flushed the primitive has to check if any new revision has been created so
////       the entry has to be aware of the hashtable current value because it has to check if a new value is available
////       and in case discard the old one
//
//#define STORAGE_KVDB_SHARD_BLOCK_MAX_SIZE (64 * 1024)
//#define STORAGE_KVDB_SHARD_VERSION 1
//#define STORAGE_KVDB_SHARD_MAGIC_NUMBER_HIGH 0x4341434845475241
//#define STORAGE_KVDB_SHARD_MAGIC_NUMBER_LOW  0x5241000000000000
//
//// TODO: write interface to manage the kvdb index
////storage_kvdb_t *storage_kvdb_create_index();
////storage_kvdb_t *storage_kvdb_load_index();
////storage_kvdb_t *storage_kvdb_flush_index();
//
//typedef struct storage_kvdb storage_kvdb_t;
//
//typedef struct storage_kvdb_block_offset storage_kvdb_block_offset_t;
//struct storage_kvdb_block_offset {
//    uint32_t shard_index;
//    uint32_t shard_offset;
//};
//
//typedef struct storage_kvdb_stream storage_kvdb_stream_t;
//struct storage_kvdb_stream {
//    storage_kvdb_block_offset_t *blocks_offsets;
//    uint32_t blocks_offsets_count;
//    uint32_t blocks_offsets_index;
//    bool blocks_offsets_via_slab;
//};
//
//typedef struct storage_kvdb_shard_header storage_kvdb_shard_header_t;
//struct storage_kvdb_shard_header {
//    // TODO: add general archive information ie. creation time, last write, other useful information
//    struct {
//        uint64_t high;
//        uint64_t low;
//    } magic_number;
//    uint16_t version;
//    uint32_t index;
//    size_t size;
//    struct {
//        uint64_t sec;
//        uint64_t nsec;
//    } creation_time;
//} __attribute((packed));
//
//typedef struct storage_kvdb_shard storage_kvdb_shard_t;
//struct storage_kvdb_shard {
//    char *path;
//    uint32_t index;
//    size_t size;
//    off_t offset;
//    bool finalized;
//    storage_channel_t *channel;
//    storage_kvdb_t *kvdb;
//
//    struct {
//        char *data;
//        size_t size;
//        off_t offset;
//    } write_buffer;
//
//    storage_kvdb_shard_header_t header;
//};
//
//typedef union {
//    double_linked_list_item_t double_linked_list_item;
//    struct {
//        void* padding[2];
//        storage_kvdb_shard_t *shard;
//    } data;
//} storage_kvdb_shard_list_item_t;
//
//typedef double_linked_list_t storage_kvdb_shard_list_t;
//
//struct storage_kvdb {
//    char *basedir_path;
//    size_t shard_size;
//    storage_kvdb_shard_t *current_shard;
//
//    // TODO: need an hashtable to quickly search & find a shard by index instead of iterating the list, without the
//    //       local cache performances will be killed
//    storage_kvdb_shard_list_t *shards;
//};
//
//char *storage_kvdb_shard_build_path(
//        char *basedir_path,
//        uint32_t shard_index) {
//    char *path;
//
//    int basedir_path_len = (int)strlen(basedir_path);
//    if (basedir_path[basedir_path_len - 1] == '/') {
//        basedir_path_len--;
//    }
//
//    // TODO: pretty inefficient to use snprintf here to build the path but the archive is created only when an archive
//    //       rotation is requested so currently it's not a big deal
//    size_t required_length = snprintf(
//            NULL,
//            0,
//            "%.*s/kvdb-%d.shard",
//            basedir_path_len,
//            basedir_path,
//            shard_index);
//    path = slab_allocator_mem_alloc(required_length + 1);
//
//    snprintf(
//            path,
//            required_length + 1,
//            "%.*s/kvdb-%d.shard",
//            basedir_path_len,
//            basedir_path,
//            shard_index);
//
//    return path;
//}
//
//storage_kvdb_shard_t *storage_kvdb_shard_new(
//        storage_kvdb_t *storage_kvdb,
//        uint32_t shard_index) {
//    storage_kvdb_shard_t *shard = slab_allocator_mem_alloc_zero(sizeof(storage_kvdb_shard_t));
//    shard->kvdb = storage_kvdb;
//    shard->index = shard_index;
//    shard->path = storage_kvdb_shard_build_path(
//            storage_kvdb->basedir_path,
//            shard_index);
//
//    return shard;
//}
//
//void storage_kvdb_shard_create_init_write_buffer(
//        storage_kvdb_shard_t *storage_kvdb_shard) {
//    storage_kvdb_shard->write_buffer.data = slab_allocator_mem_alloc(STORAGE_KVDB_SHARD_BLOCK_MAX_SIZE);
//}
//
//void storage_kvdb_shard_create_prepare_header(
//        storage_kvdb_shard_t *storage_kvdb_shard,
//        size_t shard_size,
//        timespec_t *timespec) {
//    // Fill up the header of the strut and sync it to disk
//    storage_kvdb_shard->header.magic_number.high = STORAGE_KVDB_SHARD_MAGIC_NUMBER_HIGH;
//    storage_kvdb_shard->header.magic_number.low = STORAGE_KVDB_SHARD_MAGIC_NUMBER_LOW;
//    storage_kvdb_shard->header.version = STORAGE_KVDB_SHARD_VERSION;
//    storage_kvdb_shard->header.index = storage_kvdb_shard->index;
//    storage_kvdb_shard->header.size = shard_size;
//    storage_kvdb_shard->header.creation_time.sec = timespec->tv_sec;
//    storage_kvdb_shard->header.creation_time.nsec = timespec->tv_nsec;
//}
//
//storage_channel_t *storage_kvdb_shard_open_or_create(
//        char *path,
//        bool create) {
//    timespec_t timespec;
//    clock_monotonic(&timespec);
//
//    storage_channel_t *storage_channel = storage_open(
//            path,
//            (create ? (O_CREAT | O_EXCL) : 0) | O_RDWR,
//            0);
//
//    return storage_channel;
//}
//
//bool storage_kvdb_shard_create_readwrite(
//        storage_kvdb_shard_t *storage_kvdb_shard,
//        size_t shard_size) {
//    timespec_t timespec;
//    clock_monotonic(&timespec);
//
//     if ((storage_kvdb_shard->channel = storage_kvdb_shard_open_or_create(
//             storage_kvdb_shard->path,
//             true)) == NULL) {
//         LOG_E(
//                 TAG,
//                 "[SHARD %d] Failed to create a new shard",
//                 storage_kvdb_shard->index);
//         return false;
//     }
//
//    if (!storage_fallocate(
//            storage_kvdb_shard->channel,
//            FALLOC_FL_KEEP_SIZE,
//            0,
//            (long)shard_size)) {
//        LOG_E(
//                TAG,
//                "[SHARD %d] Failed to set the shard size to <%lu> bytes",
//                storage_kvdb_shard->index,
//                shard_size);
//        return false;
//    }
//
//    storage_kvdb_shard->size = shard_size;
//
//    storage_kvdb_shard_create_init_write_buffer(
//            storage_kvdb_shard);
//    storage_kvdb_shard_create_prepare_header(
//            storage_kvdb_shard,
//            shard_size,
//            &timespec);
//
//    // Copy the data in the buffer and update the buffer offset so the data will be synced on the disk with the first
//    // write
//    memcpy(storage_kvdb_shard->write_buffer.data, &storage_kvdb_shard->header, sizeof(storage_kvdb_shard->header));
//    storage_kvdb_shard->write_buffer.offset += sizeof(storage_kvdb_shard->header);
//
//    return true;
//}
//
////bool storage_kvdb_shard_open_read(
////        storage_kvdb_shard_t *storage_kvdb_shard) {
////    timespec_t timespec;
////    clock_monotonic(&timespec);
////
////    if ((storage_kvdb_shard->channel = storage_kvdb_shard_open_or_create(
////            storage_kvdb_shard->path,
////            false)) == NULL) {
////        LOG_E(
////                TAG,
////                "[SHARD %d] Failed to open the shard",
////                storage_kvdb_shard->index);
////        return false;
////    }
////
////    // TODO: read the header
////    // TODO: validate the magic number
////    // TODO: validate if it's a supported version
////    // TODO: validate that the index in storage_kvdb_shard matches the one in the header
////
////    return true;
////}
//
//bool storage_kvdb_shard_flush(
//        storage_kvdb_shard_t *storage_kvdb_shard) {
//    bool res = true;
//    if (storage_kvdb_shard->write_buffer.offset > 0) {
//        storage_io_common_iovec_t iovec[1] = {
//                {
//                    .iov_base = storage_kvdb_shard->write_buffer.data,
//                    .iov_len = storage_kvdb_shard->write_buffer.offset
//                }
//        };
//
//        res = storage_writev(
//                storage_kvdb_shard->channel,
//                iovec,
//                1,
//                storage_kvdb_shard->write_buffer.offset,
//                storage_kvdb_shard->offset);
//
//        if (!res) {
//            LOG_E(
//                    TAG,
//                    "[SHARD %d] Failed to flush the <%lu> bytes in the write buffer to disk",
//                    storage_kvdb_shard->index,
//                    storage_kvdb_shard->write_buffer.offset);
//        }
//        storage_kvdb_shard->write_buffer.offset = 0;
//    }
//
//    return res;
//}
//
//bool storage_kvdb_shard_write(
//        storage_kvdb_shard_t *storage_kvdb_shard,
//        char *data,
//        size_t data_length) {
//    assert(data_length < STORAGE_KVDB_SHARD_BLOCK_MAX_SIZE);
//    assert(storage_kvdb_shard->finalized == false);
//
//    ssize_t remaining_size = (ssize_t)((data_length + storage_kvdb_shard->write_buffer.offset) - storage_kvdb_shard->write_buffer.size);
//    if (remaining_size < 0) {
//        remaining_size = 0;
//    }
//    ssize_t can_copy_size = (ssize_t)(data_length - remaining_size);
//
//    memcpy(storage_kvdb_shard->write_buffer.data, data, can_copy_size);
//    storage_kvdb_shard->write_buffer.offset += can_copy_size;
//
//    if (storage_kvdb_shard->write_buffer.offset == storage_kvdb_shard->write_buffer.size) {
//        if (!storage_kvdb_shard_flush(storage_kvdb_shard)) {
//            return false;
//        }
//    }
//
//    if (remaining_size > 0) {
//        memcpy(storage_kvdb_shard->write_buffer.data, data + can_copy_size, remaining_size);
//        storage_kvdb_shard->write_buffer.offset += remaining_size;
//    }
//
//    return true;
//}
//
//bool storage_kvdb_shard_has_enough_space(
//        storage_kvdb_shard_t *storage_kvdb_shard,
//        size_t data_length) {
//    return data_length + storage_kvdb_shard->write_buffer.offset <= storage_kvdb_shard->write_buffer.size;
//}
//
//bool storage_kvdb_shard_finalize(
//        storage_kvdb_shard_t *storage_kvdb_shard) {
////    // TODO: write finalization record, needed to reload the data quickly
////    storage_kvdb_shard_write(
////            storage_kvdb_shard,);
//
//    if (!storage_kvdb_shard_flush(storage_kvdb_shard)) {
//        return false;
//    }
//
//    storage_kvdb_shard->finalized = true;
//
//    return true;
//}
//
//bool storage_kvdb_shard_close(
//        storage_kvdb_shard_t *storage_kvdb_shard) {
//    assert(storage_kvdb_shard->finalized == true);
//
//    if (storage_close(storage_kvdb_shard->channel) == false) {
//        LOG_E(
//                TAG,
//                "[SHARD %d] Failed to close the shard",
//                storage_kvdb_shard->index);
//    }
//
//    storage_kvdb_shard->channel = NULL;
//    return true;
//}
//
//void storage_kvdb_shard_free(
//        storage_kvdb_shard_t *storage_kvdb_shard) {
//
//    slab_allocator_mem_free(storage_kvdb_shard->path);
//
//    if (storage_kvdb_shard->write_buffer.data) {
//        slab_allocator_mem_free(storage_kvdb_shard->write_buffer.data);
//    }
//
//    slab_allocator_mem_free(storage_kvdb_shard);
//}
//
//storage_kvdb_t *storage_kvdb_new(
//        char *basedir_path,
//        size_t shard_size) {
//    storage_kvdb_t *kvdb = slab_allocator_mem_alloc_zero(sizeof(storage_kvdb_t));
//
//    kvdb->basedir_path = basedir_path;
//    kvdb->shard_size = shard_size;
//    kvdb->shards = double_linked_list_init();
//
//    return kvdb;
//}
//
//bool storage_kvdb_write_grow_shards(
//        storage_kvdb_t* storage_kvdb) {
//    uint32_t shard_index = 0;
//
//    if (storage_kvdb->current_shard) {
//        shard_index = storage_kvdb->current_shard->index + 1;
//    }
//
//    storage_kvdb_shard_t *shard = storage_kvdb_shard_new(
//            storage_kvdb,
//            shard_index);
//    if (!shard) {
//        LOG_E(TAG, "Failed to allocate memory for a new shard");
//        return false;
//    }
//
//    if (!storage_kvdb_shard_create_readwrite(
//            shard,
//            storage_kvdb->shard_size)) {
//        storage_kvdb_shard_free(shard);
//
//        LOG_E(TAG, "Failed to create a new shard");
//        return false;
//    }
//
//    storage_kvdb->current_shard = shard;
//
//    storage_kvdb_shard_list_item_t *item = (storage_kvdb_shard_list_item_t*)double_linked_list_item_init();
//    item->data.shard = shard;
//
//    double_linked_list_push_item(
//            storage_kvdb->shards,
//            (double_linked_list_item_t *)item);
//
//    return true;
//}
//
//bool storage_kvdb_write_need_to_grow_shards(
//        storage_kvdb_t* storage_kvdb,
//        size_t data_length) {
//    if (!storage_kvdb->current_shard) {
//        return true;
//    }
//
//    return storage_kvdb_shard_has_enough_space(
//            storage_kvdb->current_shard,
//            data_length);
//}
//
//bool storage_kvdb_write(
//        storage_kvdb_t* storage_kvdb,
//        char *data,
//        size_t data_length) {
//    if (storage_kvdb_write_need_to_grow_shards(
//            storage_kvdb,
//            data_length)) {
//        if (!storage_kvdb_write_grow_shards(
//                storage_kvdb)) {
//            return false;
//        }
//    }
//
//    return storage_kvdb_shard_write(
//            storage_kvdb->current_shard,
//            data,
//            data_length);
//}
//
//void storage_kvdb_free(storage_kvdb_t* storage_kvdb) {
//    double_linked_list_item_t* item = NULL;
//
//    while((item = double_linked_list_iter_next(storage_kvdb->shards, item)) != NULL) {
//        // TODO: ensure that all the shard are flushed to the disk and closed
//        storage_kvdb_shard_list_item_t *item_shard = (storage_kvdb_shard_list_item_t*)item;
//        storage_kvdb_shard_free(item_shard->data.shard);
//    }
//
//    double_linked_list_free(storage_kvdb->shards);
//    slab_allocator_mem_free(storage_kvdb);
//}
//
//storage_kvdb_stream_t *storage_kvdb_stream_new(
//        size_t data_length) {
//    assert(STORAGE_KVDB_SHARD_BLOCK_MAX_SIZE <= SLAB_OBJECT_SIZE_MAX);
//    storage_kvdb_stream_t *stream = slab_allocator_mem_alloc_zero(sizeof(storage_kvdb_stream_t));
//
//    stream->blocks_offsets_count = (data_length + (STORAGE_KVDB_SHARD_BLOCK_MAX_SIZE - 1)) / STORAGE_KVDB_SHARD_BLOCK_MAX_SIZE;
//    size_t blocks_offset_size = stream->blocks_offsets_count * sizeof(storage_kvdb_block_offset_t);
//    if (blocks_offset_size > SLAB_OBJECT_SIZE_MAX) {
//        stream->blocks_offsets = xalloc_alloc(blocks_offset_size);
//        stream->blocks_offsets_via_slab = false;
//    } else {
//        stream->blocks_offsets = slab_allocator_mem_alloc(blocks_offset_size);
//        stream->blocks_offsets_via_slab = false;
//    }
//
//
//    return stream;
//}
//
//bool storage_kvdb_stream_flush(
//        storage_kvdb_stream_t *storage_kvdb_stream) {
//}
//
//bool storage_kvdb_stream_write(
//        storage_kvdb_stream_t *storage_kvdb_stream,
//        char* buffer,
//        size_t buffer_length) {
//
//}
//
//bool storage_kvdb_stream_end(
//        storage_kvdb_stream_t *storage_kvdb_stream) {
//
//}
//
//void storage_kvdb_stream_free(
//        storage_kvdb_stream_t *storage_kvdb_stream) {
//    if (storage_kvdb_stream->blocks_offsets_via_slab) {
//        slab_allocator_mem_free(storage_kvdb_stream->blocks_offsets);
//    } else {
//        xalloc_free(storage_kvdb_stream->blocks_offsets);
//    }
//    slab_allocator_mem_free(storage_kvdb_stream->write_buffer);
//
//    slab_allocator_mem_free(storage_kvdb_stream);
//}
