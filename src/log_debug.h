#ifndef CACHEGRAND_LOG_DEBUG_H
#define CACHEGRAND_LOG_DEBUG_H

#if DEBUG == 1

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

#define LOG_MESSAGE_DEBUG_RULES_SRC_PATH_EXCLUDE

#define LOG_MESSAGE_DEBUG_RULES_SRC_FUNC_INCLUDE

#define LOG_MESSAGE_DEBUG_RULES_SRC_FUNC_EXCLUDE \
        "hashtable_support_op_search_key", \
        "hashtable_op_get", \
        "hashtable_support_op_search_key_or_create_new", \
        "hashtable_op_set", \
        "hashtable_support_op_half_hashes_chunk_lock" , \
        "hashtable_support_op_half_hashes_chunk_unlock",

#define LOG_MESSAGE_DEBUG_RULES(var, TYPE, INCLUDE_OR_EXCLUDE) \
    const char* var[] = { LOG_MESSAGE_DEBUG_RULES_##TYPE##_##INCLUDE_OR_EXCLUDE NULL }

bool log_message_debug_check_rules(const char* str, const char* rules_include[], const char* rules_exclude[]);
void log_message_debug(const char* src_path, const char* src_func, const int src_line, const char* message, ...);

#ifdef __cplusplus
}
#endif

#endif // DEBUG == 1

#endif //CACHEGRAND_LOG_DEBUG_H
