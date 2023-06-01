#ifndef CACHEGRAND_STORAGE_DB_COUNTERS_H
#define CACHEGRAND_STORAGE_DB_COUNTERS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#define STORAGE_DB_COUNTERS_UPDATE(storage_db, database_number, ...) { \
    storage_db_counters_t *counters; \
    { \
        counters = &storage_db_counters_get_current_thread_data( \
            storage_db)->global; \
        __VA_ARGS__ \
    } \
    { \
        counters = storage_db_counters_per_thread_get_or_create( \
            storage_db, \
            database_number); \
        __VA_ARGS__ \
    } \
}

extern pthread_key_t storage_db_counters_index_key;

void storage_db_counters_slot_key_init_once();

void storage_db_counters_slot_key_ensure_init(
        storage_db_t *storage_db);

void storage_db_counters_sum_global(
        storage_db_t *storage_db,
        storage_db_counters_t *counters);

void storage_db_counters_sum_per_db(
        storage_db_t *storage_db,
        storage_db_database_number_t database_number,
        storage_db_counters_t *counters);


static inline __attribute__((always_inline)) uint64_t storage_db_counters_get_current_thread_get_slot_index(
        storage_db_t *storage_db) {
    storage_db_counters_slot_key_ensure_init(storage_db);

    void *value = pthread_getspecific(storage_db_counters_index_key);
    storage_db_counters_slots_bitmap_and_index_t *slot =
            (storage_db_counters_slots_bitmap_and_index_t*)value;

    return slot->index;
}

static inline __attribute__((always_inline)) storage_db_counters_global_and_per_db_t* storage_db_counters_get_current_thread_data(
        storage_db_t *storage_db) {
    return &storage_db->counters[storage_db_counters_get_current_thread_get_slot_index(storage_db)];
}

static inline storage_db_counters_t* storage_db_counters_per_thread_get_or_create(
        storage_db_t *storage_db,
        storage_db_database_number_t database_number) {
    storage_db_counters_t *counters_per_db = NULL;

    storage_db_counters_global_and_per_db_t *counters_global_and_per_db =
            storage_db_counters_get_current_thread_data(storage_db);

    counters_per_db = (storage_db_counters_t*) hashtable_spsc_op_get_by_hash_and_key_uint32(
            counters_global_and_per_db->per_db,
            database_number,
            database_number);

    if (!counters_per_db) {
        counters_per_db = ffma_mem_alloc_zero(sizeof(storage_db_counters_t));
        hashtable_spsc_op_try_set_by_hash_and_key_uint32(
                counters_global_and_per_db->per_db,
                database_number,
                database_number,
                counters_per_db);
    }

    return counters_per_db;
}

#endif //CACHEGRAND_STORAGE_DB_COUNTERS_H
