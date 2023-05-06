#ifndef CACHEGRAND_STORAGE_BUFFERED_H
#define CACHEGRAND_STORAGE_BUFFERED_H

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_BUFFERED_PAGE_SIZE (4 * 1024)

off_t storage_buffered_get_offset(
        storage_buffered_channel_t *storage_buffered_channel);

void storage_buffered_set_offset(
        storage_buffered_channel_t *storage_buffered_channel,
        off_t offset);

bool storage_buffered_flush_write(
        storage_buffered_channel_t *storage_buffered_channel);

size_t storage_buffered_read_buffer_acquire_slice(
        storage_buffered_channel_t *storage_buffered_channel,
        size_t slice_length,
        storage_buffered_channel_buffer_data_t **buffer_out);

void storage_buffered_read_buffer_release_slice(
        storage_buffered_channel_t *storage_buffered_channel);

static inline __attribute__((always_inline)) storage_buffered_channel_buffer_data_t *storage_buffered_write_buffer_acquire_slice(
        storage_buffered_channel_t *storage_buffered_channel,
        size_t slice_length) {
    // Ensure that the slice requested can fit into the buffer and that there isn't already a slice acquired
    assert(slice_length <= storage_buffered_channel->buffers.write.buffer.length);
    assert(storage_buffered_channel->buffers.write.slice_acquired_length == 0);

    // Check if there is enough space on the buffer, if not flush it
    if (unlikely(storage_buffered_channel->buffers.write.buffer.data_size + slice_length >
                 storage_buffered_channel->buffers.write.buffer.length)) {
        if (unlikely(!storage_buffered_flush_write(storage_buffered_channel))) {
            return NULL;
        }
    }

#if DEBUG == 1
    storage_buffered_channel->buffers.write.slice_acquired_length = slice_length;
#endif

    return storage_buffered_channel->buffers.write.buffer.data +
           storage_buffered_channel->buffers.write.buffer.data_offset;
}

static inline __attribute__((always_inline))void storage_buffered_write_buffer_release_slice(
        storage_buffered_channel_t *storage_buffered_channel,
        size_t slice_used_length) {
    // Ensure that when the slice is released, the amount of data used is always the same or smaller than the length
    // acquired. Also ensure that there was a slice acquired.
    assert(storage_buffered_channel->buffers.write.slice_acquired_length > 0);
    assert(slice_used_length <= storage_buffered_channel->buffers.write.slice_acquired_length);

    storage_buffered_channel->buffers.write.buffer.data_size += slice_used_length;
    storage_buffered_channel->buffers.write.buffer.data_offset += slice_used_length;

#if DEBUG == 1
    storage_buffered_channel->buffers.write.slice_acquired_length = 0;
#endif
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_BUFFERED_H
