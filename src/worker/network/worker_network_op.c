/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
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

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "config.h"
#include "log/log.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "worker/worker_op.h"
#include "worker/worker_fiber.h"
#include "support/simple_file_io.h"
#include "fiber/fiber_scheduler.h"
#include "network/network.h"
#include "network/network_tls_mbedtls.h"
#include "network/network_tls.h"
#include "network/channel/network_channel_tls.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"

#include "worker_network_op.h"

#define TAG "worker_network_op"

// Network operations
worker_op_network_channel_new_fp_t* worker_op_network_channel_new;
worker_op_network_channel_multi_new_fp_t* worker_op_network_channel_multi_new;
worker_op_network_channel_multi_get_fp_t* worker_op_network_channel_multi_get;
worker_op_network_channel_multi_free_fp_t* worker_op_network_channel_multi_free;
worker_op_network_channel_size_fp_t* worker_op_network_channel_size;
worker_op_network_channel_free_fp_t* worker_op_network_channel_free;
worker_op_network_accept_fp_t* worker_op_network_accept;
worker_op_network_receive_fp_t* worker_op_network_receive;
worker_op_network_receive_timeout_fp_t* worker_op_network_receive_timeout;
worker_op_network_send_fp_t* worker_op_network_send;
worker_op_network_close_fp_t* worker_op_network_close;

worker_module_context_t *worker_module_contexts_initialize(
        config_t *config) {
    bool result_ret = false;
    worker_module_context_t *worker_module_context = NULL;

    worker_module_context =
            xalloc_alloc_zero(sizeof(worker_module_context_t) * config->modules_count);
    if (!worker_module_context) {
        goto end;
    }

    for(int module_index = 0; module_index < config->modules_count; module_index++) {
        config_module_t *config_module = &config->modules[module_index];

        if (config_module->network->tls == NULL) {
            continue;
        }

        size_t cipher_suites_ids_size;
        int *cipher_suites = network_tls_build_cipher_suites_from_names(
                config_module->network->tls->cipher_suites,
                config_module->network->tls->cipher_suites_count,
                &cipher_suites_ids_size);

        network_tls_config_t *network_tls_config = network_tls_config_init(
                config_module->network->tls->certificate_path,
                config_module->network->tls->private_key_path,
                config_module->network->tls->ca_certificate_chain_path,
                config_module->network->tls->min_version,
                config_module->network->tls->max_version,
                cipher_suites,
                cipher_suites_ids_size,
                config_module->network->tls->verify_client_certificate);

        if (cipher_suites) {
            xalloc_free(cipher_suites);
        }

        if (!network_tls_config) {
            goto end;
        }

        worker_module_context[module_index].network_tls_config = network_tls_config;
    }

    result_ret = true;
end:

    if (!result_ret && worker_module_context) {
        for (int module_index = 0; module_index < config->modules_count; module_index++) {
            if (worker_module_context[module_index].network_tls_config == NULL) {
                continue;
            }

            network_tls_config_free(worker_module_context[module_index].network_tls_config);
        }

        xalloc_free(worker_module_context);
        worker_module_context = NULL;
    }

    return worker_module_context;
}

void worker_module_context_free(
        config_t *config,
        worker_module_context_t *worker_module_context) {
    for (int module_index = 0; module_index < config->modules_count; module_index++) {
        if (worker_module_context == NULL || worker_module_context[module_index].network_tls_config == NULL) {
            continue;
        }
        network_tls_config_free(worker_module_context[module_index].network_tls_config);
    }

    xalloc_free(worker_module_context);
}

// TODO: the listener and accept operations should be refactored to split them in an user frontend operation and in an
//       internal operation like for all the other ops (recv, send, close, etc.)
bool worker_network_listeners_initialize(
        uint32_t worker_index,
        uint8_t core_index,
        config_t *config,
        worker_module_context_t *worker_module_context,
        network_channel_t **listeners,
        uint8_t *listeners_count) {
    bool return_res = false;
    network_channel_listener_new_callback_user_data_t listener_new_cb_user_data = { 0 };

    listener_new_cb_user_data.core_index = core_index;

    // With listeners = NULL, the number of needed listeners will be enumerated and listeners_count
    // increased as needed
    listener_new_cb_user_data.listeners = NULL;
    for(int module_index = 0; module_index < config->modules_count; module_index++) {
        config_module_t *config_module = &config->modules[module_index];
        for(int binding_index = 0; binding_index < config_module->network->bindings_count; binding_index++) {
            if (network_channel_listener_new(
                    config_module->network->bindings[binding_index].host,
                    config_module->network->bindings[binding_index].port,
                    config->network->listen_backlog,
                    0,
                    &listener_new_cb_user_data) == false) {
                return_res = false;
                goto end;
            }
        }
    }

    // Allocate the needed listeners and reset listeners_count
    listener_new_cb_user_data.listeners = worker_op_network_channel_multi_new(
            NETWORK_CHANNEL_TYPE_LISTENER,
            listener_new_cb_user_data.listeners_count);
    listener_new_cb_user_data.network_channel_size = worker_op_network_channel_size();
    listener_new_cb_user_data.listeners_count = 0;

    // Allocate the listeners (with the correct protocol config)
    for(int module_index = 0; module_index < config->modules_count; module_index++) {
        config_module_t *config_module = &config->modules[module_index];
        module_id_t module_id = config_module->module_id;

        for(int binding_index = 0; binding_index < config_module->network->bindings_count; binding_index++) {
            config_module_network_binding_t *binding = &config_module->network->bindings[binding_index];
            uint8_t listeners_count_before = listener_new_cb_user_data.listeners_count;
            if (network_channel_listener_new(
                    binding->host,
                    binding->port,
                    config->network->listen_backlog,
                    module_id,
                    &listener_new_cb_user_data) == false) {

                LOG_E(TAG, "Unable to setup listener for <%s:%u> with protocol <%d>",
                      binding->host,
                      binding->port,
                      module_id);

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
                channel_listener->module_config = config_module;

                // If TLS is enabled for the binding, import the TLS settings
                if (binding->tls) {
                    network_tls_config_t *network_tls_config =
                            worker_module_context[module_index].network_tls_config;
                    network_channel_tls_set_config(
                            channel_listener,
                            &network_tls_config->config);
                    network_channel_tls_set_enabled(
                            channel_listener,
                            worker_module_context[module_index].network_tls_config == NULL ? false : true);
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

bool worker_network_listeners_listen(
        worker_context_t *worker_context,
        network_channel_t *listeners,
        uint8_t listeners_count) {
    for (int listener_index = 0; listener_index < listeners_count; listener_index++) {
        if (!worker_fiber_register(
                worker_context,
                "worker-fiber-listener",
                worker_network_listeners_fiber_entrypoint,
                (void *)worker_op_network_channel_multi_get(
                        listeners,
                        listener_index))) {
            LOG_E(TAG, "Failed to register listener fiber");
            return false;
        }
    }

    return true;
}

void worker_network_new_client_fiber_entrypoint(
        void *user_data) {
    worker_stats_t *stats = worker_stats_get_internal_current();

    network_channel_t *new_channel = user_data;
    bool tls_enabled = new_channel->tls.enabled;
    bool tls_handshake_completed = false;

    stats->network.active_connections++;
    stats->network.accepted_connections++;

    if (tls_enabled) {
        if (unlikely(!network_channel_tls_init(new_channel))) {
            LOG_W(
                    TAG,
                    "[FD:%5d] TLS setup failed for the connection <%s>",
                    new_channel->fd,
                    new_channel->address.str);
            goto end;
        }

        if (unlikely(!network_channel_tls_handshake(new_channel))) {
            LOG_V(
                    TAG,
                    "[FD:%5d] TLS handshake failed for the connection <%s>",
                    new_channel->fd,
                    new_channel->address.str);
            goto end;
        }

        tls_handshake_completed = true;
        stats->network.active_tls_connections++;
        stats->network.accepted_tls_connections++;

        if (network_channel_tls_ktls_supports_mbedtls_cipher_suite(new_channel)) {
            LOG_D(
                    TAG,
                    "kTLS supports the cipher, it can be enabled for the connection <%s>",
                    new_channel->address.str);
            if (network_channel_tls_setup_ktls(new_channel)) {
                // Enable kTLS and ensure mbedtls is disabled
                network_channel_tls_set_ktls(
                        new_channel,
                        true);
                network_channel_tls_set_mbedtls(
                        new_channel,
                        false);

                LOG_D(
                        TAG,
                        "kTLS successfully enabled for connection <%s>",
                        new_channel->address.str);
            } else {
                LOG_D(
                        TAG,
                        "Failed to enable kTLS for the connection <%s>, switching to mbedtls",
                        new_channel->address.str);
            }
        }

        // If kTLS can't be enabled or its activation fails, enable mbedtls
        if (!network_channel_tls_uses_ktls(new_channel)) {
            network_channel_tls_set_mbedtls(
                    new_channel,
                    true);
        }

        // If the client has sent a client certificate, report the common name in the logs
        if (network_channel_tls_has_peer_certificate(new_channel)) {
            const char *cn = NULL;
            size_t cn_length = 0;

            if (network_channel_tls_peer_certificate_get_cn(
                    new_channel,
                    &cn,
                    &cn_length)) {
                LOG_D(
                        TAG,
                        "TLS client certificate common name: %.*s",
                        (int)cn_length,
                        cn);
            }
        }
    }

    // Handover the new connection to the module
    module_get_by_id(new_channel->module_id)->connection_accept(new_channel);

end:
    // TODO: when it gets here new_channel might have been already freed, the flow should always close the connection
    //       the connection here and not from within the module
    if (new_channel->status != NETWORK_CHANNEL_STATUS_CLOSED) {
        network_close(new_channel, true);
    }

    // Updates the amount of active connections
    stats->network.active_connections--;

    // Check if tls was enabled at the beginning of the connection, as the channel, when it gets closed, has its
    // properties, including tls.enabled, reset to the default values
    if (tls_enabled && tls_handshake_completed) {
        stats->network.active_tls_connections--;
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
            if (!worker_should_terminate(worker_context)) {
                LOG_V(
                        TAG,
                        "[FD:%5d][ACCEPT] Listener <%s> failed to accept a new connection",
                        listener_channel->fd,
                        listener_channel->address.str);
            }

            continue;
        }

        new_channel->status = NETWORK_CHANNEL_STATUS_CONNECTED;

        // TODO: should implement a wrapper for this
        int64_t read_ms = new_channel->module_config->network->timeout->read_ms;
        int64_t write_ms = new_channel->module_config->network->timeout->write_ms;
        new_channel->timeout.read.sec = read_ms > 0 ? read_ms / 1000 : -1;
        new_channel->timeout.read.nsec = read_ms > 0 ? (read_ms % 1000) * 1000000 : -1;
        new_channel->timeout.write.sec = write_ms > 0 ? write_ms / 1000 : -1;
        new_channel->timeout.write.nsec = write_ms > 0 ? (write_ms % 1000) * 1000000 : -1;

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
