#ifndef CACHEGRAND_NETWORK_IO_COMMON_H
#define CACHEGRAND_NETWORK_IO_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int network_io_common_fd_t;

typedef bool (*network_io_common_socket_setup_server_cb_t)(
        network_io_common_fd_t fd,
        void* user_data);

typedef bool (*network_io_common_parse_addresses_foreach_callback_t)(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t port,
        uint16_t backlog,
        network_protocols_t protocol,
        void* user_data);

bool network_io_common_socket_set_option(
        network_io_common_fd_t fd,
        int level,
        int option,
        void* value,
        socklen_t value_size);
bool network_io_common_socket_set_reuse_address(
        network_io_common_fd_t fd,
        bool enable);
bool network_io_common_socket_set_reuse_port(
        network_io_common_fd_t fd,
        bool enable);
bool network_io_common_socket_attach_reuseport_cbpf(
        network_io_common_fd_t fd);
bool network_io_common_socket_set_nodelay(
        network_io_common_fd_t fd,
        bool enable);
bool network_io_common_socket_set_quickack(
        network_io_common_fd_t fd,
        bool enable);
bool network_io_common_socket_set_linger(
        network_io_common_fd_t fd,
        bool enable,
        int seconds);
bool network_io_common_socket_set_keepalive(
        network_io_common_fd_t fd,
        bool enable);
bool network_io_common_socket_set_incoming_cpu(
        network_io_common_fd_t fd,
        int cpu);
bool network_io_common_socket_set_receive_buffer(
        network_io_common_fd_t fd,
        int size);
bool network_io_common_socket_set_send_buffer(
        network_io_common_fd_t fd,
        int size);
bool network_io_common_socket_set_receive_timeout(
        network_io_common_fd_t fd,
        long seconds,
        long useconds);
bool network_io_common_socket_set_send_timeout(
        network_io_common_fd_t fd,
        long seconds,
        long useconds);
bool network_io_common_socket_set_ipv6_only(
        network_io_common_fd_t fd,
        bool enable);

bool network_io_common_socket_bind(
        network_io_common_fd_t fd,
        struct sockaddr *address,
        socklen_t address_size);
bool network_io_common_socket_listen(
        network_io_common_fd_t fd,
        uint16_t backlog);
bool network_io_common_socket_close(
        network_io_common_fd_t fd,
        bool shutdown_may_fail);

bool network_io_common_socket_setup_server(
        network_io_common_fd_t fd,
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

int32_t network_io_common_parse_addresses_foreach(
        char *address,
        uint16_t port,
        uint16_t backlog,
        network_io_common_parse_addresses_foreach_callback_t callback,
        network_protocols_t protocol,
        void* user_data);

char* network_io_common_socket_address_str(
        struct sockaddr* address,
        char* buffer,
        size_t buffer_len);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_IO_COMMON_H
