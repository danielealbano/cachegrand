/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <linux/tls.h>
#include <linux/filter.h>

#include "misc.h"
#include "log/log.h"

#include "network/protocol/network_protocol.h"
#include "network_io_common.h"
#include "network_io_common_tls.h"

#define TAG "network_io_common_tls"

bool network_io_common_tls_socket_set_ulp(
        network_io_common_fd_t fd,
        char *ulp) {
    return network_io_common_socket_set_option(fd, SOL_TCP, TCP_ULP, ulp, strlen(ulp));
}

bool network_io_common_tls_socket_set_tls_rx(
        network_io_common_fd_t fd,
        network_io_common_tls_crypto_info_t *val,
        size_t length) {
    return network_io_common_socket_set_option(fd, SOL_TLS, TLS_RX, val, length);
}

bool network_io_common_tls_socket_set_tls_tx(
        network_io_common_fd_t fd,
        network_io_common_tls_crypto_info_t *val,
        size_t length) {
    return network_io_common_socket_set_option(fd, SOL_TLS, TLS_TX, val, length);
}
