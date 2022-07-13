#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_SUPPORT_RANDOM_KEYS_MIN_LENGTH             11
#define TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH             19
#define TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL   (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH + 1)
#define TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_REPEATED_LIST \
    'q','w','e','r','t','y','u','i','o','p','a','s','d','f','g','h','j','k', 'l','z','x','c','v','b','n','m', \
    'q','w','e','r','t','y','u','i','o','p','a','s','d','f','g','h','j','k', 'l','z','x','c','v','b','n','m', \
    'Q','W','E','R','T','Y','U','I','O','P','A','S','D','F','G','H','J','K', 'L','Z','X','C','V','B','N','M', \
    'Q','W','E','R','T','Y','U','I','O','P','A','S','D','F','G','H','J','K', 'L','Z','X','C','V','B','N','M', \
    '1','2','3','4','5','6','7','8','9','0', '1','2','3','4','5','6','7','8','9','0', \
    '.',',','/','|','\'',';',']','[','<','>','?',':','"','{','}','!','@','$','%','^','&','*','(',')','_','-','=','+','#'

#define TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_UNIQUE_LIST \
    'q','w','e','r','t','y','u','i','o','p','a','s','d','f','g','h','j','k', 'l','z','x','c','v','b','n','m', \
    'Q','W','E','R','T','Y','U','I','O','P','A','S','D','F','G','H','J','K', 'L','Z','X','C','V','B','N','M', \
    '1','2','3','4','5','6','7','8','9','0', \
    '.',',','/','|','\'',';',']','[','<','>','?',':','"','{','}','!','@','$','%','^','&','*','(',')','_','-','=','+','#'

#define TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_REPEATED_SIZE  sizeof((char[]){TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_REPEATED_LIST})
#define TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_UNIQUE_SIZE  sizeof((char[]){TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_UNIQUE_LIST})

#define TEST_SUPPORT_RANDOM_KEYS_GEN_FUNC_RANDOM_STR_MAX_LENGTH                 1
#define TEST_SUPPORT_RANDOM_KEYS_GEN_FUNC_RANDOM_STR_RANDOM_LENGTH              2
// #define TEST_SUPPORT_RANDOM_KEYS_GEN_FUNC_REPETIBLE_STR_ALTERNATEMINMAX_LENGTH  3

#define TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(DATA, DATA_LEN, FIXTURE_PATH, ...) { \
    { \
        char FIXTURE_PATH[] = "/tmp/cachegrand-tests-XXXXXX.tmp"; \
        int FIXTURE_PATH ## _suffix_len = 4; /** .tmp **/ \
        test_support_fixture_file_from_data_create(FIXTURE_PATH, FIXTURE_PATH ## _suffix_len, DATA, DATA_LEN); \
        __VA_ARGS__; \
        test_support_fixture_file_from_data_cleanup(FIXTURE_PATH); \
    } \
}

// We don't really want to import the hashtable stuff in all the tests
typedef struct hashtable hashtable_t;

#if HASHTABLE_USE_UINT64 == 1
typedef uint64_t hashtable_bucket_index_t;
typedef uint64_t hashtable_chunk_index_t;
#else
typedef uint32_t hashtable_bucket_index_t;
typedef uint32_t hashtable_chunk_index_t;
#endif
typedef uint8_t hashtable_key_value_flags_t;
typedef uint64_t hashtable_hash_t;
typedef uint32_t hashtable_hash_half_t;
typedef uint16_t hashtable_hash_quarter_t;

typedef uint8_t hashtable_chunk_slot_index_t;
typedef hashtable_bucket_index_t hashtable_bucket_count_t;
typedef hashtable_chunk_index_t hashtable_chunk_count_t;
typedef uint32_t hashtable_key_size_t;
typedef char hashtable_key_data_t;
typedef uintptr_t hashtable_value_data_t;

typedef struct test_key_same_bucket test_key_same_bucket_t;
struct test_key_same_bucket {
    char* key;
    hashtable_key_size_t key_len;
    hashtable_hash_t key_hash;
    hashtable_hash_half_t key_hash_half;
    hashtable_hash_quarter_t key_hash_quarter;
};

typedef struct keyset_generator_thread_info keyset_generator_thread_info_t;
struct keyset_generator_thread_info {
    char* charset_list;
    size_t charset_size;
    uint64_t random_seed_base;
    uint8_t* start_flag;
    pthread_t thread_id;
    uint16_t thread_num;
    uint16_t threads_count;
    uint64_t keyset_size;
    char* keyset;
    uint64_t start;
    uint64_t end;
};

void test_support_hashtable_print_heatmap(
        hashtable_t* hashtable,
        uint8_t columns);

test_key_same_bucket_t* test_support_same_hash_mod_fixtures_generate(
        hashtable_bucket_count_t bucket_count,
        const char* key_prefix,
        uint32_t count);

void test_support_same_hash_mod_fixtures_free(
        test_key_same_bucket_t* test_key_same_bucket);

void test_support_set_thread_affinity(
        int thread_index);

void test_support_free_keys(
        char* keys,
        uint64_t count);

char* test_support_init_keys(
        uint64_t keys_count,
        uint8_t keys_generator_method,
        uint64_t random_seed_base);

hashtable_t* test_support_init_hashtable(
        uint64_t initial_size);

bool bench_support_check_if_too_many_threads_per_core(
        int threads,
        int max_per_core);

void bench_support_set_thread_affinity(
        int thread_index);

bool test_support_hashtable_prefill(
        hashtable_t* hashtable,
        char* keyset,
        uint64_t value,
        uint64_t insert_count);

void test_support_flush_data_cache(
        void *start,
        size_t len);

bool test_support_fixture_file_from_data_create(
        char* path,
        int path_suffix_len,
        const char* data,
        size_t data_len);

void test_support_fixture_file_from_data_cleanup(
        const char* path);

#ifdef __cplusplus
}
#endif

#endif //TEST_SUPPORT_H
