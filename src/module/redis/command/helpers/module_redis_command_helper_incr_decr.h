#ifndef CACHEGRAND_MODULE_REDIS_COMMAND_HELPER_INCR_DECR_H
#define CACHEGRAND_MODULE_REDIS_COMMAND_HELPER_INCR_DECR_H

#ifdef __cplusplus
extern "C" {
#endif

bool module_redis_command_helper_incr_decr(
        module_redis_connection_context_t *connection_context,
        int64_t amount,
        char **key,
        size_t *key_length);

bool module_redis_command_helper_incr_decr_float(
        module_redis_connection_context_t *connection_context,
        long double amount,
        char **key,
        size_t *key_length);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_COMMAND_HELPER_INCR_DECR_H
