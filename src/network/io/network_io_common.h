#ifndef CACHEGRAND_NETWORK_IO_COMMON_H
#define CACHEGRAND_NETWORK_IO_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*network_io_common_socket_setup_server_cb_t)(
        int fd,
        void* user_data);

typedef bool (*network_io_common_parse_addresses_foreach_callback_t)(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t socket_address_index,
        void* user_data);

bool network_io_common_socket_set_option(
        int fd,
        int level,
        int option,
        void* value,
        socklen_t value_size);
bool network_io_common_socket_set_reuse_address(
        int fd,
        bool enable);
bool network_io_common_socket_set_reuse_port(
        int fd,
        bool enable);
bool network_io_common_socket_set_linger(
        int fd,
        bool enable,
        int seconds);
bool network_io_common_socket_set_keepalive(
        int fd,
        bool enable);
bool network_io_common_socket_set_incoming_cpu(
        int fd,
        int cpu);
bool network_io_common_socket_set_receive_buffer(
        int fd,
        int size);
bool network_io_common_socket_set_send_buffer(
        int fd,
        int size);
bool network_io_common_socket_set_receive_timeout(
        int fd,
        long seconds,
        long useconds);
bool network_io_common_socket_set_send_timeout(
        int fd,
        long seconds,
        long useconds);
bool network_io_common_socket_set_ipv6_only(
        int fd,
        bool enable);

bool network_io_common_socket_bind(
        int fd,
        struct sockaddr *address,
        socklen_t address_size);
bool network_io_common_socket_listen(
        int fd,
        uint16_t backlog);
bool network_io_common_socket_close(
        int fd,
        bool shutdown_may_fail);

bool network_io_common_socket_setup_server(
        int fd,
        struct sockaddr *address,
        socklen_t address_size,
        uint16_t backlog,
        network_io_common_socket_setup_server_cb_t socket_setup_server_cb,
        void *user_data);

int network_io_common_socket_tcp4_new(
        int flags);
int network_io_common_socket_tcp4_new_server(
        int flags,
        struct sockaddr_in *address,
        uint16_t backlog,
        network_io_common_socket_setup_server_cb_t socket_setup_server_cb,
        void *user_data);

int network_io_common_socket_tcp6_new(
        int flags);
int network_io_common_socket_tcp6_new_server(
        int flags,
        struct sockaddr_in6 *address,
        uint16_t backlog,
        network_io_common_socket_setup_server_cb_t socket_setup_server_cb,
        void *user_data);

int network_io_common_socket_new_server(
        int family,
        int flags,
        struct sockaddr *socket_address,
        uint16_t port,
        uint16_t backlog,
        network_io_common_socket_setup_server_cb_t socket_setup_server_cb,
        void *user_data);

uint32_t network_io_common_parse_addresses_foreach(
        char *address,
        network_io_common_parse_addresses_foreach_callback_t callback,
        void* user_data);

char* network_io_common_socket_address_str(
        struct sockaddr* address,
        char* buffer,
        size_t buffer_len);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_IO_COMMON_H
