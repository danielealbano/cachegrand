/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <math.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "utils_string.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "module/module.h"
#include "config.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/network.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/redis/module_redis.h"
#include "module/redis/module_redis_connection.h"
#include "worker/worker_op.h"

#include "module_redis_command_helper_config.h"

#define TAG "module_redis_command_helper_config"

hashtable_spsc_t *module_redis_command_helper_config_parameters_hashtable = NULL;

FUNCTION_CTOR(module_redis_command_helper_config_ctor, {
    uint32_t params_count = sizeof(module_redis_command_helper_config_parameters) / sizeof(module_redis_command_helper_config_parameters[0]);

    module_redis_command_helper_config_parameters_hashtable = hashtable_spsc_new(
            params_count,
            32,
            false);
    if (!module_redis_command_helper_config_parameters_hashtable) {
        FATAL(TAG, "Unable to generate the config params hashtable");
    }

    for(uint32_t param_index = 0; param_index < params_count; param_index++) {
        if (!hashtable_spsc_op_try_set_ci(
                module_redis_command_helper_config_parameters_hashtable,
                module_redis_command_helper_config_parameters[param_index].name,
                strlen(module_redis_command_helper_config_parameters[param_index].name),
                &module_redis_command_helper_config_parameters[param_index])) {

            FATAL(
                    TAG,
                    "Unable to set the config param <%s>",
                    module_redis_command_helper_config_parameters[param_index].name);
        }
    }
});

FUNCTION_DTOR(module_redis_command_helper_config_dtor, {
    hashtable_spsc_free(module_redis_command_helper_config_parameters_hashtable);
});

module_redis_command_helper_config_parameter_key_value_t *module_redis_command_helper_config_parameter_get(
        char *key,
        size_t key_length) {
    return hashtable_spsc_op_get_ci(
            module_redis_command_helper_config_parameters_hashtable,
            key,
            key_length);
}

char *module_redis_command_config_get_handle_parameter_value(
        config_t *config,
        config_module_t *config_module,
        storage_db_config_t *storage_db_config,
        char *parameter_name) {
    if (strcmp(parameter_name, "bind") == 0) {
        // cachegrand supports multiple bind addresses, but redis only one so we send only the first one
        size_t size = strlen(config_module->network->bindings[0].host);
        char *host = xalloc_alloc(size + 1);
        strncpy(host, config_module->network->bindings[0].host, size + 1);

        return host;
    }

    if (strcmp(parameter_name, "port") == 0) {
        // cachegrand supports multiple bind addresses, but redis only one so we send only the first one that is not
        // marked as tls
        uint16_t port = 0;
        for (uint64_t bindings_index = 0; bindings_index < config_module->network->bindings_count; bindings_index++) {
            if (!config_module->network->bindings[bindings_index].tls) {
                port = config_module->network->bindings[bindings_index].port;
            }
        }

        size_t size = snprintf(NULL, 0, "%d", port);
        char *port_str = xalloc_alloc(size + 1);
        snprintf(port_str, size + 1, "%d", port);

        return port_str;
    }

    if (strcmp(parameter_name, "tls-port") == 0) {
        // cachegrand supports multiple bind addresses, but redis only one so we send only the first one that is
        // marked as tls
        uint16_t tls_port = 0;
        for (uint64_t bindings_index = 0; bindings_index < config_module->network->bindings_count; bindings_index++) {
            if (config_module->network->bindings[bindings_index].tls) {
                tls_port = config_module->network->bindings[bindings_index].port;
            }
        }

        size_t size = snprintf(NULL, 0, "%d", tls_port);
        char *port_str = xalloc_alloc(size + 1);
        snprintf(port_str, size + 1, "%d", tls_port);

        return port_str;
    }

    if (strcmp(parameter_name, "client-query-buffer-limit") == 0) {
        size_t size = snprintf(NULL, 0, "%u", config_module->redis->max_command_length);
        char *databases_str = xalloc_alloc(size + 1);
        snprintf(databases_str, size + 1, "%u", config_module->redis->max_command_length);

        return databases_str;
    }

    if (strcmp(parameter_name, "crash-log-enabled") == 0) {
        size_t size = 4;
        char *crash_log_enabled_str = xalloc_alloc(size + 1);
        if (config->sentry->enable) {
            snprintf(crash_log_enabled_str, size + 1, "yes");
        } else {
            snprintf(crash_log_enabled_str, size + 1, "no");
        }

        return crash_log_enabled_str;
    }

    if (strcmp(parameter_name, "databases") == 0) {
        size_t size = snprintf(NULL, 0, "%ld", config->database->max_user_databases);
        char *databases_str = xalloc_alloc(size + 1);
        snprintf(databases_str, size + 1, "%ld", config->database->max_user_databases);

        return databases_str;
    }

    if (strcmp(parameter_name, "dbfilename") == 0) {
        char *path;

        if (config->database->snapshots) {
            size_t size = strlen(config->database->snapshots->path);
            path = xalloc_alloc(size + 1);
            strncpy(path, config->database->snapshots->path, size + 1);
        } else {
            size_t size = 0;
            path = xalloc_alloc(size + 1);
            path[0] = '\0';
        }

        return path;
    }

    if (strcmp(parameter_name, "maxclients") == 0) {
        size_t size = snprintf(NULL, 0, "%u", config->network->max_clients);
        char *maxclients_str = xalloc_alloc(size + 1);
        snprintf(maxclients_str, size + 1, "%u", config->network->max_clients);

        return maxclients_str;
    }

    if (strcmp(parameter_name, "maxmemory-policy") == 0) {
        size_t size = 15;
        char *policy_str = xalloc_alloc(size + 1);

        if (config->database->keys_eviction) {
            switch (config->database->keys_eviction->policy) {
                case CONFIG_DATABASE_KEYS_EVICTION_POLICY_LRU:
                    if (config->database->keys_eviction->only_ttl) {
                        strncpy(policy_str, "volatile-lru", size + 1);
                    } else {
                        strncpy(policy_str, "allkeys-lru", size + 1);
                    }
                    break;

                case CONFIG_DATABASE_KEYS_EVICTION_POLICY_LFU:
                    if (config->database->keys_eviction->only_ttl) {
                        strncpy(policy_str, "volatile-lfu", size + 1);
                    } else {
                        strncpy(policy_str, "allkeys-lfu", size + 1);
                    }
                    break;

                case CONFIG_DATABASE_KEYS_EVICTION_POLICY_RANDOM:
                    if (config->database->keys_eviction->only_ttl) {
                        strncpy(policy_str, "volatile-random", size + 1);
                    } else {
                        strncpy(policy_str, "allkeys-random", size + 1);
                    }
                    break;

                case CONFIG_DATABASE_KEYS_EVICTION_POLICY_TTL:
                    strncpy(policy_str, "volatile-ttl", size + 1);
                    break;
            }
        } else {
            strncpy(policy_str, "noeviction", size + 1);
        }

        return policy_str;
    }

    if (strcmp(parameter_name, "maxmemory-samples") == 0) {
        size_t size = snprintf(
                NULL,
                0,
                "%d",
                STORAGE_DB_KEYS_EVICTION_BITONIC_SORT_16_ELEMENTS_ARRAY_LENGTH);
        char *maxmemory_samples_str = xalloc_alloc(size + 1);
        snprintf(
                maxmemory_samples_str,
                size + 1,
                "%d",
                STORAGE_DB_KEYS_EVICTION_BITONIC_SORT_16_ELEMENTS_ARRAY_LENGTH);

        return maxmemory_samples_str;
    }

    if (strcmp(parameter_name, "maxmemory") == 0) {
        size_t size = snprintf(NULL, 0, "%ld", storage_db_config->limits.data_size.hard_limit);
        char *maxmemory_str = xalloc_alloc(size + 1);
        snprintf(maxmemory_str, size + 1, "%ld", storage_db_config->limits.data_size.hard_limit);

        return maxmemory_str;
    }

    if (strcmp(parameter_name, "proto-max-bulk-len") == 0) {
        size_t size = snprintf(NULL, 0, "%u", config_module->redis->max_command_length);
        char *proto_max_bulk_len_str = xalloc_alloc(size + 1);
        snprintf(proto_max_bulk_len_str, size + 1, "%u", config_module->redis->max_command_length);

        return proto_max_bulk_len_str;
    }

    assert(0 && "Parameter not handled");
}

bool module_redis_command_helper_config_send_response(
        module_redis_connection_context_t *connection_context,
        module_redis_command_helper_config_parameter_key_value_t *parameters,
        uint64_t parameters_count) {
    bool return_res = false;
    network_channel_buffer_data_t *send_buffer, *send_buffer_start, *send_buffer_end;

    size_t slice_length = 64 + (parameters_count * 64);
    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(
            connection_context->network_channel,
            slice_length);
    if (send_buffer_start == NULL) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        goto end;
    }

    send_buffer_end = send_buffer + slice_length;

    if (connection_context->resp_version == PROTOCOL_REDIS_RESP_VERSION_2) {
        send_buffer_start = protocol_redis_writer_write_array(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                parameters_count * 2);
    } else {
        send_buffer_start = protocol_redis_writer_write_map(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                parameters_count);
    }

    if (send_buffer_start == NULL) {
        network_send_buffer_release_slice(
                connection_context->network_channel,
                0);
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        goto end;
    }

    for(uint64_t parameter_index = 0; parameter_index < parameters_count; parameter_index++) {
        module_redis_command_helper_config_parameter_key_value_t *parameter = &parameters[parameter_index];

        send_buffer_start = protocol_redis_writer_write_blob_string(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                parameter->name,
                (int)strlen(parameter->name));

        if (send_buffer_start == NULL) {
            network_send_buffer_release_slice(
                    connection_context->network_channel,
                    0);
            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
            goto end;
        }

        send_buffer_start = protocol_redis_writer_write_blob_string(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                (char*)parameter->value,
                (int)strlen(parameter->value));

        if (send_buffer_start == NULL) {
            network_send_buffer_release_slice(
                    connection_context->network_channel,
                    0);
            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
            goto end;
        }
    }

    network_send_buffer_release_slice(
            connection_context->network_channel,
            send_buffer_start - send_buffer);

    return_res = true;
    end:

    return return_res;
}
