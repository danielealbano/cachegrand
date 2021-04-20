#ifndef CACHEGRAND_NETWORK_CHANNEL_IOURING_H
#define CACHEGRAND_NETWORK_CHANNEL_IOURING_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int network_chanell_iouring_mapped_fd_t;

enum network_io_iouring_op {
    NETWORK_IO_IOURING_OP_UNKNOWN = 0,
    NETWORK_IO_IOURING_OP_NOP,
    NETWORK_IO_IOURING_OP_FILES_UPDATE,
    NETWORK_IO_IOURING_OP_ACCEPT,
    NETWORK_IO_IOURING_OP_RECV,
    NETWORK_IO_IOURING_OP_SEND,

    // This goes for last
    NETWORK_IO_IOURING_OP_LAST,
};
typedef enum network_io_iouring_op network_io_iouring_op_t;

typedef struct network_channel_iouring_entry_user_data network_channel_iouring_entry_user_data_t;
struct network_channel_iouring_entry_user_data {
    network_io_iouring_op_t op;
    network_chanell_iouring_mapped_fd_t mapped_fd;
    network_channel_t *channel;
    network_channel_socket_address_t listener_new_socket_address;
};

network_channel_iouring_entry_user_data_t* network_channel_iouring_entry_user_data_new(
        network_io_iouring_op_t op);

network_channel_iouring_entry_user_data_t* network_channel_iouring_entry_user_data_new_with_mapped_fd(
        network_io_iouring_op_t op,
        network_chanell_iouring_mapped_fd_t mapped_fd);

void network_channel_iouring_entry_user_data_free(
        network_channel_iouring_entry_user_data_t* iouring_user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_CHANNEL_IOURING_H
