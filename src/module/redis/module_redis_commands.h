#ifndef CACHEGRAND_MODULE_REDIS_COMMANDS_H
#define CACHEGRAND_MODULE_REDIS_COMMANDS_H

#ifdef __cplusplus
extern "C" {
#endif

extern hashtable_spsc_t *module_redis_commands_hashtable;

void module_redis_commands_set_disabled_commands_hashtables(
        hashtable_spsc_t *commands_hashtable);

hashtable_spsc_t *module_redis_commands_get_disabled_commands_hashtables();

hashtable_spsc_t *module_redis_commands_build_disabled_commands_hashtables(
        char **disabled_commands,
        uint32_t disabled_commands_count);

bool module_redis_commands_is_command_disabled(
        char *command,
        size_t command_length);

void module_redis_commands_free_disabled_commands_hashtables(
        hashtable_spsc_t *hashtable);

hashtable_spsc_t *module_redis_commands_build_commands_hashtables(
        module_redis_command_info_t *command_infos,
        uint32_t command_infos_count);

uint16_t module_redis_commands_count_tokens_in_command_arguments(
        module_redis_command_argument_t *arguments,
        uint16_t arguments_count);

void module_redis_commands_build_command_argument_token_entry_oneof_tokens(
        module_redis_command_argument_t *argument,
        module_redis_command_parser_context_argument_token_entry_t *token_entry);

module_redis_command_parser_context_argument_token_entry_t *module_redis_commands_build_command_argument_token_entry(
        module_redis_command_argument_t *argument);

bool module_redis_commands_build_command_arguments_token_entries_hashtable(
        module_redis_command_argument_t *arguments,
        uint16_t arguments_count,
        hashtable_spsc_t *hashtable);

bool module_redis_commands_build_commands_arguments_token_entries_hashtable(
        module_redis_command_info_t *command_infos,
        uint32_t command_infos_count);

void module_redis_commands_free_command_arguments_token_entries_hashtable(
        module_redis_command_info_t *command_info);

void module_redis_commands_free_commands_arguments_token_entries_hashtable(
        module_redis_command_info_t *command_infos,
        uint32_t command_infos_count);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_COMMANDS_H
