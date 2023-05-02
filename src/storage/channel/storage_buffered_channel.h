#ifndef CACHEGRAND_STORAGE_BUFFERED_CHANNEL_H
#define CACHEGRAND_STORAGE_BUFFERED_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_BUFFERED_CHANNEL_BUFFER_SIZE ((64 * 1024))

typedef char storage_buffered_channel_buffer_data_t;

typedef struct storage_buffered_channel_buffer storage_buffered_channel_buffer_t;
struct storage_buffered_channel_buffer {
    storage_buffered_channel_buffer_data_t *data;
    size_t data_offset;
    size_t data_size;
    size_t length;
};

typedef struct storage_buffered_channel storage_buffered_channel_t;
struct storage_buffered_channel {
    storage_channel_t *storage_channel;
    off_t offset;
    struct {
        struct {
            storage_buffered_channel_buffer_t buffer;
#if DEBUG == 1
            size_t slice_acquired_length;
#endif
        } read;
        struct {
            storage_buffered_channel_buffer_t buffer;
#if DEBUG == 1
            size_t slice_acquired_length;
#endif
        } write;
    } buffers;
};

storage_buffered_channel_t* storage_buffered_channel_new(
        storage_channel_t *storage_channel);

void storage_buffered_channel_free(
        storage_buffered_channel_t* storage_buffered_channel);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_BUFFERED_CHANNEL_H
