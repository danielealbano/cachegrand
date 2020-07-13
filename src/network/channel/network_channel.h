#ifndef CACHEGRAND_NETWORK_CHANNEL_H
#define CACHEGRAND_NETWORK_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_CHANNEL_PACKET_BUFFER_SIZE  2048
#define NETWORK_CHANNEL_LISTENERS_MAX       16

typedef struct network_channel_address network_channel_address_t;
struct network_channel_address {
    char* address;
    uint16_t port;
};

typedef struct network_channel_listener network_channel_listener_t;
struct network_channel_listener {
    int fd;
    union {
        struct sockaddr base;
        struct sockaddr_in ipv4;
        struct sockaddr_in6 ipv6;
    } address;
    socklen_t address_size;
};

// TODO: decouple the list of listeners from the listener new callback user_data
typedef struct network_create_lister_new_user_data network_channel_listener_new_callback_user_data_t;
struct network_create_lister_new_user_data {
    uint16_t port;
    uint16_t backlog;
    uint8_t core_index;
    uint8_t listeners_count;
    network_channel_listener_t listeners[NETWORK_CHANNEL_LISTENERS_MAX];
};

bool network_channel_client_setup(
        int fd,
        int incoming_cpu);
bool network_channel_server_setup(
        int fd,
        int incoming_cpu);
bool network_channel_listener_new_callback(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t socket_address_index,
        void* user_data);
bool network_channel_listener_new(
        char* address,
        uint16_t port,
        network_channel_listener_new_callback_user_data_t *user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_CHANNEL_H
