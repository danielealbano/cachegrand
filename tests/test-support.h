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
#define TEST_SUPPORT_RANDOM_KEYS_GEN_FUNC_REPETIBLE_STR_ALTERNATEMINMAX_LENGTH  3

typedef struct test_key_same_bucket test_key_same_bucket_t;
struct test_key_same_bucket {
    char* key;
    hashtable_key_size_t key_len;
    hashtable_hash_t key_hash;
    hashtable_hash_half_t key_hash_half;
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

#ifdef __cplusplus
}
#endif

#endif //TEST_SUPPORT_H
