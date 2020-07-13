#ifndef CACHEGRAND_NETWORK_CHANNEL_IOURING_H
#define CACHEGRAND_NETWORK_CHANNEL_IOURING_H

#ifdef __cplusplus
extern "C" {
#endif

// TODO: this enum and the struct below should most likely become more generic because they should be shared between
//       the storage io and the network io, will see
// Never use 0 as OP, helps to spot issues.
enum {
    NETWORK_IO_IOURING_OP_NOP = 1,
    NETWORK_IO_IOURING_OP_FILES_UPDATE,
    NETWORK_IO_IOURING_OP_ACCEPT,
    NETWORK_IO_IOURING_OP_RECV,
    NETWORK_IO_IOURING_OP_SEND,

    // This goes for last
    NETWORK_IO_IOURING_OP_LAST,
};

typedef struct network_channel_iouring_entry_user_data network_channel_iouring_entry_user_data_t;
struct network_channel_iouring_entry_user_data {
    int32_t fd;
    uint8_t op;
    char* buffer;
    union {
        struct sockaddr base;
        struct sockaddr_in ipv4;
        struct sockaddr_in6 ipv6;
    } new_socket_address;
    char address_str[INET6_ADDRSTRLEN + 1];
    socklen_t address_size;
};

network_channel_iouring_entry_user_data_t* network_channel_iouring_entry_user_data_new(
        uint32_t op);
network_channel_iouring_entry_user_data_t* network_channel_iouring_entry_user_data_new_with_fd(
        uint32_t op,
        uint32_t fd);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_CHANNEL_IOURING_H
