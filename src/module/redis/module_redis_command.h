#ifndef CACHEGRAND_MODULE_REDIS_COMMAND_H
#define CACHEGRAND_MODULE_REDIS_COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

module_redis_command_context_t* module_redis_command_alloc_context(
        module_redis_command_info_t *command_info);

void module_redis_command_free_context(
        module_redis_command_info_t *command_info,
        module_redis_command_context_t *command_context);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_COMMAND_H
