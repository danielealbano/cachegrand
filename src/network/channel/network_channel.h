#ifndef CACHEGRAND_NETWORK_CHANNEL_H
#define CACHEGRAND_NETWORK_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_CHANNEL_PACKET_SIZE  (8 * 1024)

// The NETWORK_CHANNEL_RECV_BUFFER_SIZE has to be twice the NETWORK_CHANNEL_PACKET_SIZE to ensure that it's always
// possible to read a full packet in addition to any partially received data while processing the buffer and that there
// is enough room to avoid copying continuously the data at the beginning (a streaming parser is being used so there
// maybe data that need still to be parsed)
#define NETWORK_CHANNEL_RECV_BUFFER_SIZE    (NETWORK_CHANNEL_PACKET_SIZE * 2)

// Do not lower, to improve the performances the code expects to be able to send up to this amount of data, and do
// not increase as the slab allocator supports only up to 64kb.
#define NETWORK_CHANNEL_SEND_BUFFER_SIZE    (64 * 1024)

typedef char network_channel_buffer_data_t;

enum network_channel_type {
    NETWORK_CHANNEL_TYPE_LISTENER,
    NETWORK_CHANNEL_TYPE_CLIENT,
};
typedef enum network_channel_type network_channel_type_t;

enum network_channel_status {
    NETWORK_CHANNEL_STATUS_UNDEFINED,
    NETWORK_CHANNEL_STATUS_CONNECTED,
    NETWORK_CHANNEL_STATUS_CLOSED
};
typedef enum network_channel_status network_channel_status_t;

typedef struct network_channel_address network_channel_address_t;
struct network_channel_address {
    char* address;
    uint16_t port;
    module_types_t protocol;
};

typedef struct network_channel_socket_address network_channel_socket_address_t;
struct network_channel_socket_address {
    union {
        struct sockaddr base;
        struct sockaddr_in ipv4;
        struct sockaddr_in6 ipv6;
    } socket;
    char str[INET6_ADDRSTRLEN + 1 + 5 + 1]; // ipv6 + : + port number + null
    socklen_t size;
};

typedef struct network_channel_buffer network_channel_buffer_t;
struct network_channel_buffer {
    network_channel_buffer_data_t *data;
    size_t data_offset;
    size_t data_size;
    size_t length;
};

typedef struct network_channel network_channel_t;
struct network_channel {
    network_io_common_fd_t fd;
    network_channel_type_t type;
    module_types_t protocol;
    config_module_t *module_config;
    network_channel_socket_address_t address;
    network_channel_status_t status;
    struct {
        network_channel_buffer_t send;
#if DEBUG == 1
        size_t send_slice_acquired_length;
#endif
    } buffers;
    struct {
        bool enabled;
        bool ktls;
        bool mbedtls;
        void *context;
        void *config;
    } tls;
    struct {
        struct {
            int64_t sec;
            int64_t nsec;
        } read;
        struct {
            int64_t sec;
            int64_t nsec;
        } write;
    } timeout;
};

typedef struct network_create_lister_new_user_data network_channel_listener_new_callback_user_data_t;
struct network_create_lister_new_user_data {
    uint8_t core_index;
    uint8_t listeners_count;
    network_channel_t *listeners;
    size_t network_channel_size;
};

bool network_channel_client_setup(
        network_io_common_fd_t fd,
        uint32_t incoming_cpu);

bool network_channel_server_setup(
        network_io_common_fd_t fd,
        uint32_t incoming_cpu);

bool network_channel_init(
        network_channel_type_t type,
        network_channel_t *channel);

void network_channel_cleanup(
        network_channel_t *channel);

bool network_channel_listener_new_callback(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t port,
        uint16_t backlog,
        module_types_t protocol,
        void* user_data);

bool network_channel_listener_new(
        char* address,
        uint16_t port,
        uint16_t backlog,
        module_types_t protocol,
        network_channel_listener_new_callback_user_data_t *user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_CHANNEL_H
