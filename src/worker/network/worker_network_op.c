/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <string.h>
#include <fatal.h>

#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/gcm.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_internal.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "slab_allocator.h"
#include "config.h"
#include "log/log.h"
#include "fiber.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "worker/worker_op.h"
#include "slab_allocator.h"
#include "support/simple_file_io.h"
#include "fiber_scheduler.h"
#include "network/network.h"
#include "network/network_tls_mbedtls.h"
#include "network/network_tls.h"
#include "network/channel/network_channel_tls.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/redis/network_protocol_redis.h"
#include "network/protocol/prometheus/network_protocol_prometheus.h"

#include "worker_network_op.h"

#define TAG "worker_network_op"

// Network operations
worker_op_network_channel_new_fp_t* worker_op_network_channel_new;
worker_op_network_channel_multi_new_fp_t* worker_op_network_channel_multi_new;
worker_op_network_channel_multi_get_fp_t* worker_op_network_channel_multi_get;
worker_op_network_channel_size_fp_t* worker_op_network_channel_size;
worker_op_network_channel_free_fp_t* worker_op_network_channel_free;
worker_op_network_accept_fp_t* worker_op_network_accept;
worker_op_network_receive_fp_t* worker_op_network_receive;
worker_op_network_send_fp_t* worker_op_network_send;
worker_op_network_close_fp_t* worker_op_network_close;

worker_network_protocol_context_t *worker_network_protocol_contexts_initialize(
        config_network_t *config_network) {
    bool result_ret = false;
    worker_network_protocol_context_t *worker_network_protocol_context = NULL;

    worker_network_protocol_context =
            slab_allocator_mem_alloc_zero(sizeof(worker_network_protocol_context_t) * config_network->protocols_count);
    if (!worker_network_protocol_context) {
        goto end;
    }

    for(int protocol_index = 0; protocol_index < config_network->protocols_count; protocol_index++) {
        config_network_protocol_t *config_network_protocol = &config_network->protocols[protocol_index];

        if (config_network_protocol->tls == NULL) {
            continue;
        }

        size_t cipher_suites_ids_size;
        int *cipher_suites = network_tls_build_cipher_suites_from_names(
                config_network_protocol->tls->cipher_suites,
                config_network_protocol->tls->cipher_suites_count,
                &cipher_suites_ids_size);

        network_tls_config_t *network_tls_config = network_tls_config_init(
                config_network_protocol->tls->certificate_path,
                config_network_protocol->tls->private_key_path,
                config_network_protocol->tls->min_version,
                config_network_protocol->tls->max_version,
                cipher_suites,
                cipher_suites_ids_size);

        if (cipher_suites) {
            slab_allocator_mem_free(cipher_suites);
        }

        if (!network_tls_config) {
            goto end;
        }

        worker_network_protocol_context[protocol_index].network_tls_config = network_tls_config;
    }

    result_ret = true;
end:

    if (!result_ret && worker_network_protocol_context) {
        for (int protocol_index = 0; protocol_index < config_network->protocols_count; protocol_index++) {
            if (worker_network_protocol_context[protocol_index].network_tls_config == NULL) {
                continue;
            }

            network_tls_config_free(worker_network_protocol_context[protocol_index].network_tls_config);
        }

        slab_allocator_mem_free(worker_network_protocol_context);
        worker_network_protocol_context = NULL;
    }

    return worker_network_protocol_context;
}

void worker_network_protocol_context_free(
        config_network_t *config_network,
        worker_network_protocol_context_t *worker_network_protocol_context) {
    for (int protocol_index = 0; protocol_index < config_network->protocols_count; protocol_index++) {
        if (worker_network_protocol_context[protocol_index].network_tls_config == NULL) {
            continue;
        }
        network_tls_config_free(worker_network_protocol_context[protocol_index].network_tls_config);
    }

    slab_allocator_mem_free(worker_network_protocol_context);
}

// TODO: the listener and accept operations should be refactored to split them in an user frontend operation and in an
//       internal operation like for all the other ops (recv, send, close, etc.)
bool worker_network_listeners_initialize(
        uint32_t worker_index,
        uint8_t core_index,
        config_network_t *config_network,
        worker_network_protocol_context_t *worker_network_protocol_context,
        network_channel_t **listeners,
        uint8_t *listeners_count) {
    bool return_res = false;
    network_channel_listener_new_callback_user_data_t listener_new_cb_user_data = { 0 };

    listener_new_cb_user_data.core_index = core_index;

    // With listeners = NULL, the number of needed listeners will be enumerated and listeners_count
    // increased as needed
    listener_new_cb_user_data.listeners = NULL;
    for(int protocol_index = 0; protocol_index < config_network->protocols_count; protocol_index++) {
        config_network_protocol_t *config_network_protocol = &config_network->protocols[protocol_index];
        for(int binding_index = 0; binding_index < config_network_protocol->bindings_count; binding_index++) {
            if (network_channel_listener_new(
                    config_network_protocol->bindings[binding_index].host,
                    config_network_protocol->bindings[binding_index].port,
                    config_network->listen_backlog,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data) == false) {
                return_res = false;
                goto end;
            }
        }
    }

    // Allocate the needed listeners and reset listeners_count
    listener_new_cb_user_data.listeners =
            worker_op_network_channel_multi_new(listener_new_cb_user_data.listeners_count);
    listener_new_cb_user_data.network_channel_size = worker_op_network_channel_size();
    listener_new_cb_user_data.listeners_count = 0;

    // Allocate the listeners (with the correct protocol config)
    for(int protocol_index = 0; protocol_index < config_network->protocols_count; protocol_index++) {
        network_protocols_t network_protocol;

        config_network_protocol_t *config_network_protocol = &config_network->protocols[protocol_index];
        switch(config_network_protocol->type) {
            default:
            case CONFIG_PROTOCOL_TYPE_REDIS:
                network_protocol = NETWORK_PROTOCOLS_REDIS;
                break;

            case CONFIG_PROTOCOL_TYPE_PROMETHEUS:
                network_protocol = NETWORK_PROTOCOLS_PROMETHEUS;
                break;
        }

        for(int binding_index = 0; binding_index < config_network_protocol->bindings_count; binding_index++) {
            config_network_protocol_binding_t *binding = &config_network_protocol->bindings[binding_index];
            uint8_t listeners_count_before = listener_new_cb_user_data.listeners_count;
            if (network_channel_listener_new(
                    binding->host,
                    binding->port,
                    config_network->listen_backlog,
                    network_protocol,
                    &listener_new_cb_user_data) == false) {

                LOG_E(TAG, "Unable to setup listener for <%s:%u> with protocol <%d>",
                      binding->host,
                      binding->port,
                      network_protocol);

                return_res = false;
                goto end;
            }

            for(
                    uint8_t listener_index = listeners_count_before;
                    listener_index < listener_new_cb_user_data.listeners_count;
                    listener_index++) {
                network_channel_t *channel_listener =
                        ((void*)listener_new_cb_user_data.listeners) +
                        (listener_new_cb_user_data.network_channel_size * listener_index);
                channel_listener->protocol_config = config_network_protocol;

                // If TLS is enabled for the binding, import the TLS settings
                if (binding->tls) {
                    network_tls_config_t *network_tls_config =
                            worker_network_protocol_context[protocol_index].network_tls_config;
                    network_channel_tls_set_config(
                            channel_listener,
                            &network_tls_config->config);
                    network_channel_tls_set_enabled(
                            channel_listener,
                            worker_network_protocol_context[protocol_index].network_tls_config == NULL ? false : true);
                    network_channel_tls_set_ktls(
                            channel_listener,
                            false);
                }

                // Attach the cBPF program for reuse port only once, the worker with index 0 will always be initialized
                if (worker_index == 0) {
                    if (!network_io_common_socket_attach_reuseport_cbpf(channel_listener->fd)) {
                        LOG_E(TAG, "Failed to attach the reuse port cbpf program to the listener");
                        return false;
                    }
                }
            }
        }
    }

    return_res = true;

end:
    *listeners = listener_new_cb_user_data.listeners;
    *listeners_count = listener_new_cb_user_data.listeners_count;

    return return_res;
}

void worker_network_listeners_listen(
        fiber_t **listeners_fibers,
        network_channel_t *listeners,
        uint8_t listeners_count) {
    for (int listener_index = 0; listener_index < listeners_count; listener_index++) {
        network_channel_t *listener_channel = worker_op_network_channel_multi_get(
                listeners,
                listener_index);

        listeners_fibers[listener_index] = fiber_scheduler_new_fiber(
                "worker-listener",
                sizeof("worker-listener") - 1,
                worker_network_listeners_fiber_entrypoint,
                (void *)listener_channel);
    }
}

void worker_network_new_client_fiber_entrypoint(
        void *user_data) {
    worker_context_t *worker_context = worker_context_get();
    worker_stats_t *stats = worker_stats_get();

    network_channel_t *new_channel = user_data;
    bool tls_enabled = new_channel->tls.enabled;

    stats->network.total.active_connections++;
    stats->network.total.active_tls_connections++;

    if (stats->network.total.active_connections > worker_context->config->network->max_clients) {
        LOG_W(
                TAG,
                "[FD:%5d][ACCEPT] Maximum active connections established, can't accept any new connection",
                new_channel->fd);
        goto end;
    }

    stats->network.total.accepted_connections++;
    stats->network.per_minute.accepted_connections++;

    if (tls_enabled) {
        stats->network.total.accepted_tls_connections++;
        stats->network.per_minute.accepted_tls_connections++;
    }

    // Should not access the listener_channel directly
    switch (new_channel->protocol) {
        default:
            // This can't really happen
            FATAL(
                    TAG,
                    "[FD:%5d][ACCEPT] Channel unknown protocol <%d>",
                    new_channel->fd,
                    new_channel->protocol);
            break;

        case NETWORK_PROTOCOLS_REDIS:
            network_protocol_redis_accept(
                    new_channel);
            break;
        case NETWORK_PROTOCOLS_PROMETHEUS:
            network_protocol_prometheus_accept(
                    new_channel);
            break;
    }

end:

    // TODO: when ti gets here new_channel might have been already freed, the flow should always close the connection
    //       the connection here and not from within the module
    if (new_channel->status != NETWORK_CHANNEL_STATUS_CLOSED) {
        worker_op_network_close(new_channel, true);
    }

    // Updates the amount of active connections
    stats->network.total.active_connections--;
    if (tls_enabled) {
        stats->network.total.active_tls_connections--;
    }

    fiber_scheduler_terminate_current_fiber();
}

void worker_network_listeners_fiber_entrypoint(
        void* user_data) {
    worker_context_t *worker_context = worker_context_get();
    network_channel_t *new_channel = NULL;
    network_channel_t *listener_channel = user_data;

    while(!worker_should_terminate(worker_context)) {
        if ((new_channel = worker_op_network_accept(
                listener_channel)) == NULL) {
            LOG_V(
                    TAG,
                    "[FD:%5d][ACCEPT] Listener <%s> failed to accept a new connection",
                    listener_channel->fd,
                    listener_channel->address.str);

            continue;
        }

        new_channel->status = NETWORK_CHANNEL_STATUS_CONNECTED;

        // TODO: should implement a wrapper for this
        new_channel->timeout.read_ns = new_channel->protocol_config->timeout->read_ms > 0
                                        ? new_channel->protocol_config->timeout->read_ms * 1000000
                                        : new_channel->protocol_config->timeout->read_ms;
        new_channel->timeout.write_ns = new_channel->protocol_config->timeout->write_ms > 0
                                        ? new_channel->protocol_config->timeout->write_ms * 1000000
                                        : new_channel->protocol_config->timeout->write_ms;

        LOG_V(
                TAG,
                "[FD:%5d][ACCEPT] Listener <%s> accepting new connection from <%s>",
                listener_channel->fd,
                listener_channel->address.str,
                new_channel->address.str);
        
        fiber_scheduler_new_fiber(
                "worker-listener-client",
                strlen("worker-listener-client"),
                worker_network_new_client_fiber_entrypoint,
                (void *)new_channel);
    }

    // Switch back to the scheduler, as the lister has been closed this fiber will never be invoked and will get freed
    fiber_scheduler_switch_back();
}
