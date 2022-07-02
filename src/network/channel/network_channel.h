#ifndef CACHEGRAND_NETWORK_CHANNEL_H
#define CACHEGRAND_NETWORK_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

// TODO: with jumbo frames the macro NETWORK_CHANNEL_PACKET_SIZE would be wrong, this has to be changed to use
//       a const or a set of functions to calculate the right packet size detecting if the jumbo frames are enabled.
//       Because which interface is being used is determined via the routing table in the kernel there is no easy nor
//       direct way to fetch the right MTU so this has to come from the config and potentially if a small MTU is in use
//       and an interface is using jumbo frames a warning may be printed at the bootstrap.
//       Must also be possible to silence the warning to avoid log-spamming.
#define NETWORK_CHANNEL_PACKET_SIZE  4096

// The NETWORK_CHANNEL_RECV_BUFFER_SIZE has to be twice the NETWORK_CHANNEL_PACKET_SIZE to ensure that it's always
// possible to read a full packet in addition to any partially received data while processing the buffer and that there
// is enough room to avoid copying continuously the data at the beginning (a streaming parser is being used so there
// maybe data that need still to be parsed)
#define NETWORK_CHANNEL_RECV_BUFFER_SIZE    (NETWORK_CHANNEL_PACKET_SIZE * 2)
#define NETWORK_CHANNEL_SEND_BUFFER_SIZE    NETWORK_CHANNEL_PACKET_SIZE * 2

typedef void network_channel_state_t;
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
    network_protocols_t protocol;
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

typedef struct network_channel network_channel_t;
struct network_channel {
    network_io_common_fd_t fd;
    network_channel_type_t type;
    network_protocols_t protocol;
    config_network_protocol_t *protocol_config;
    network_channel_socket_address_t address;
    network_channel_status_t status;
    struct {
        int64_t read_ns;
        int64_t write_ns;
    } timeout;
};

typedef struct network_create_lister_new_user_data network_channel_listener_new_callback_user_data_t;
struct network_create_lister_new_user_data {
    uint8_t core_index;
    uint8_t listeners_count;
    network_channel_t *listeners;
    size_t network_channel_size;
};

typedef struct network_channel_buffer network_channel_buffer_t;
struct network_channel_buffer {
    network_channel_buffer_data_t *data;
    size_t data_offset;
    size_t data_size;
    size_t length;
};

bool network_channel_client_setup(
        network_io_common_fd_t fd,
        uint32_t incoming_cpu);

bool network_channel_server_setup(
        network_io_common_fd_t fd,
        uint32_t incoming_cpu);

bool network_channel_init(
        network_channel_t *channel);

bool network_channel_listener_new_callback(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t port,
        uint16_t backlog,
        network_protocols_t protocol,
        void* user_data);

bool network_channel_listener_new(
        char* address,
        uint16_t port,
        uint16_t backlog,
        network_protocols_t protocol,
        network_channel_listener_new_callback_user_data_t *user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_CHANNEL_H
