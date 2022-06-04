#ifndef CACHEGRAND_LOG_DEBUG_H
#define CACHEGRAND_LOG_DEBUG_H

#if DEBUG == 1
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "cmake_config.h"

#define LOG_DI(...) \
    log_message_debug(CACHEGRAND_SRC_PATH, __func__, __LINE__, __VA_ARGS__)

/**
 * The rules below have to be defined with an ending comma to allow the LOG_MESSAGE_DEBUG_RULES to work otherwise
 * it would be necessary to add some boilerplate code (ie. an ending NULL) to each set of rules.
 */

#define LOG_MESSAGE_DEBUG_RULES_SRC_PATH_INCLUDE

#define LOG_MESSAGE_DEBUG_RULES_SRC_PATH_EXCLUDE \
        "tests/test-support.c",

#define LOG_MESSAGE_DEBUG_RULES_SRC_FUNC_INCLUDE

#define LOG_MESSAGE_DEBUG_RULES_SRC_FUNC_EXCLUDE \
        "hashtable_mcmp_op_get", \
        "hashtable_mcmp_op_set", \
        "hashtable_mcmp_op_delete", \
        "hashtable_mcmp_support_op_search_key_avx512f", \
        "hashtable_mcmp_support_op_search_key_or_create_new_avx512f", \
        "hashtable_mcmp_support_op_search_key_avx2", \
        "hashtable_mcmp_support_op_search_key_or_create_new_avx2", \
        "hashtable_mcmp_support_op_search_key_avx", \
        "hashtable_mcmp_support_op_search_key_or_create_new_avx", \
        "hashtable_mcmp_support_op_search_key_sse42", \
        "hashtable_mcmp_support_op_search_key_or_create_new_sse42", \
        "hashtable_mcmp_support_op_search_key_genericx86", \
        "hashtable_mcmp_support_op_search_key_or_create_new_genericx86", \
        "hashtable_mcmp_support_op_search_key_defaultopt", \
        "hashtable_mcmp_support_op_search_key_or_create_new_defaultopt", \
        "hash_crc32c_resolve", \
        "hashtable_mcmp_support_hash_search_resolve", \
        "hashtable_mcmp_support_op_search_key_or_create_new_resolve", \
        "hashtable_mcmp_support_op_search_key_resolve", \

#define LOG_MESSAGE_DEBUG_RULES(var, TYPE, INCLUDE_OR_EXCLUDE) \
    const char* var[] = { LOG_MESSAGE_DEBUG_RULES_##TYPE##_##INCLUDE_OR_EXCLUDE NULL }

bool log_message_debug_check_rules(
        const char* str,
        const char* rules_include[],
        const char* rules_exclude[]);

void log_message_debug(
        const char* src_path,
        const char* src_func,
        const int src_line,
        const char* message,
        ...);

#ifdef __cplusplus
}
#endif

#endif // DEBUG == 1

#endif //CACHEGRAND_LOG_DEBUG_H
