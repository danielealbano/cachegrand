#ifndef CACHEGRAND_NETWORK_CHANNEL_IOURING_H
#define CACHEGRAND_NETWORK_CHANNEL_IOURING_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct network_channel_iouring network_channel_iouring_t;
struct network_channel_iouring {
    network_channel_t wrapped_channel;
    int mapped_fd;
    bool has_mapped_fd;
    int base_sqe_flags;
    int fd;
};

network_channel_iouring_t* network_channel_iouring_new();

network_channel_iouring_t* network_channel_iouring_new_multi(
        int count);

void network_channel_iouring_free(
        network_channel_iouring_t* network_channel);

    //typedef struct network_channel_iouring_entry_user_data network_channel_iouring_entry_user_data_t;
//struct network_channel_iouring_entry_user_data {
//    network_io_iouring_op_t op;
//    int mapped_fd;
//    network_channel_t *channel;
//    network_channel_socket_address_t listener_new_socket_address;
//};
//
//network_channel_iouring_entry_user_data_t* network_channel_iouring_entry_user_data_new(
//        network_io_iouring_op_t op);
//
//network_channel_iouring_entry_user_data_t* network_channel_iouring_entry_user_data_new_with_mapped_fd(
//        network_io_iouring_op_t op,
//        int mapped_fd);
//
//void network_channel_iouring_entry_user_data_free(
//        network_channel_iouring_entry_user_data_t* iouring_user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_CHANNEL_IOURING_H
