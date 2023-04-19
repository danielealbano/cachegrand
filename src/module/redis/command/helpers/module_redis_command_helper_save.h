#ifndef CACHEGRAND_MODULE_REDIS_COMMAND_HELPER_SAVE_H
#define CACHEGRAND_MODULE_REDIS_COMMAND_HELPER_SAVE_H

#ifdef __cplusplus
extern "C" {
#endif

bool module_redis_command_helper_save_is_running(
        module_redis_connection_context_t *connection_context);

uint64_t module_redis_command_helper_save_request(
        module_redis_connection_context_t *connection_context);

bool module_redis_command_helper_save_wait(
        module_redis_connection_context_t *connection_context,
        uint64_t start_time_ms);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_COMMAND_HELPER_SAVE_H
