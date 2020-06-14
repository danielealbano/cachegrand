#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_SUPPORT_RANDOM_KEYS_MIN_LENGTH              5
#define TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH              30
#define TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_LIST \
    'q','w','e','r','t','y','u','i','o','p','a','s','d','f','g','h','j','k', 'l','z','x','c','v','b','n','m', \
    'q','w','e','r','t','y','u','i','o','p','a','s','d','f','g','h','j','k', 'l','z','x','c','v','b','n','m', \
    'Q','W','E','R','T','Y','U','I','O','P','A','S','D','F','G','H','J','K', 'L','Z','X','C','V','B','N','M', \
    'Q','W','E','R','T','Y','U','I','O','P','A','S','D','F','G','H','J','K', 'L','Z','X','C','V','B','N','M', \
    '1','2','3','4','5','6','7','8','9','0', '1','2','3','4','5','6','7','8','9','0', \
    '.',',','/','|','\'',';',']','[','<','>','?',']',':','"','|','{','}','!','@','$','%','^','&','*','(',')','_','-','=','+','#'

#define TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_SIZE      sizeof((char[]){TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_LIST})
#define TEST_SUPPORT_RANDOM_KEYS_GEN_FUNC_MAX_LENGTH     1
#define TEST_SUPPORT_RANDOM_KEYS_GEN_FUNC_RANDOM_LENGTH  2

typedef struct test_key_same_bucket test_key_same_bucket_t;
struct test_key_same_bucket {
    char* key;
    hashtable_key_size_t key_len;
    hashtable_hash_t key_hash;
    hashtable_hash_half_t key_hash_half;
};

test_key_same_bucket_t* test_same_hash_mod_fixtures_generate(
        hashtable_bucket_count_t bucket_count,
        const char* key_prefix,
        uint32_t count);

void test_same_hash_mod_fixtures_free(
        test_key_same_bucket_t* test_key_same_bucket);

void test_support_set_thread_affinity(
        int thread_index);

char* test_support_build_keys_random_max_length(
        uint64_t count);

char* test_support_build_keys_random_random_length(
        uint64_t count);

void test_support_free_keys(
        char* keys,
        uint64_t count);

char* test_support_init_keys(
        uint64_t keys_count,
        uint8_t keys_generator_method);

hashtable_t* test_support_init_hashtable(
        uint64_t initial_size);

bool bench_support_check_if_too_many_threads_per_core(
        int threads,
        int max_per_core);

void bench_support_set_thread_affinity(
        int thread_index);

bool test_support_hashtable_prefill(
        hashtable_t* hashtable,
        char* keys,
        uint64_t value,
        uint64_t insert_count);

#ifdef __cplusplus
}
#endif

#endif //TEST_SUPPORT_H
