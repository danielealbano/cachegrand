#ifndef CACHEGRAND_MODULE_REDIS_COMMAND_H
#define CACHEGRAND_MODULE_REDIS_COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

bool module_redis_command_is_key_too_long(
        network_channel_t *channel,
        size_t key_length);

module_redis_command_context_t* module_redis_command_alloc_context(
        module_redis_command_info_t *command_info);

bool module_redis_command_free_context_free_argument_value_needs_free(
        module_redis_command_argument_type_t argument_type);

void module_redis_command_free_context_free_argument_value(
        module_redis_command_argument_type_t argument_type,
        void *argument_context);

void module_redis_command_free_context_free_argument(
        module_redis_command_argument_t *argument,
        void *argument_context_base_addr);

void module_redis_command_free_context_free_arguments(
        module_redis_command_argument_t *arguments,
        int arguments_count,
        void *argument_context_base_addr);

void module_redis_command_free_context(
        module_redis_command_info_t *command_info,
        module_redis_command_context_t *command_context);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_COMMAND_H
