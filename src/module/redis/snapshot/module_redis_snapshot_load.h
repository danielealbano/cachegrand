#ifndef CACHEGRAND_MODULE_REDIS_SNAPSHOT_LOAD_H
#define CACHEGRAND_MODULE_REDIS_SNAPSHOT_LOAD_H

#ifdef __cplusplus
extern "C" {
#endif

size_t module_redis_snapshot_load_read(
        storage_channel_t *channel,
        void *buffer,
        size_t length);

uint64_t module_redis_snapshot_load_read_length_encoded_int(
        storage_channel_t *channel);

void *module_redis_snapshot_load_read_string(
        storage_channel_t *channel,
        size_t *length);

bool module_redis_snapshot_load_validate_magic(
        storage_channel_t *channel);

bool module_redis_snapshot_load_validate_version(
        storage_channel_t *channel);

bool module_redis_snapshot_load_validate_checksum(
        storage_channel_t *channel);

uint8_t module_redis_snapshot_load_read_opcode(
        storage_channel_t *channel);

void module_redis_snapshot_load_process_opcode_aux(
        storage_channel_t *channel);

void module_redis_snapshot_load_process_opcode_dbnumber(
        storage_channel_t *channel);

void module_redis_snapshot_load_process_opcode_resize_db(
        storage_channel_t *channel);

uint64_t module_redis_snapshot_load_process_opcode_expire_time(
        storage_channel_t *channel,
        uint8_t opcode);

bool module_redis_snapshot_load_write_key_value_string_chunk_sequence(
        storage_db_t *db,
        char* string,
        size_t string_length,
        storage_db_chunk_sequence_t *chunk_sequence);

bool module_redis_snapshot_load_write_key_value_string(
        storage_db_t *db,
        char* key,
        size_t key_length,
        char* value,
        size_t value_length,
        uint64_t expiry_ms);

void module_redis_snapshot_load_process_value_string(
        storage_channel_t *channel,
        uint64_t expiry_ms);

void module_redis_snapshot_load_data(
        storage_channel_t *channel);

bool module_redis_snapshot_load_check_file_exists(
        char *path);

bool module_redis_snapshot_load(
        char *path);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_SNAPSHOT_LOAD_H
