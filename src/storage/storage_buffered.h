#ifndef CACHEGRAND_STORAGE_BUFFERED_H
#define CACHEGRAND_STORAGE_BUFFERED_H

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_BUFFERED_PAGE_SIZE (4 * 1024)

off_t storage_buffered_get_offset(
        storage_buffered_channel_t *storage_buffered_channel,
        off_t offset);

void storage_buffered_set_offset(
        storage_buffered_channel_t *storage_buffered_channel,
        off_t offset);

bool storage_buffered_flush_write(
        storage_buffered_channel_t *storage_buffered_channel);

storage_buffered_channel_buffer_data_t *storage_buffered_write_buffer_acquire_slice(
        storage_buffered_channel_t *storage_buffered_channel,
        size_t slice_length);

void storage_buffered_write_buffer_release_slice(
        storage_buffered_channel_t *storage_buffered_channel,
        size_t slice_used_length);

size_t storage_buffered_read_buffer_acquire_slice(
        storage_buffered_channel_t *storage_buffered_channel,
        size_t slice_length,
        storage_buffered_channel_buffer_data_t **buffer_out);

void storage_buffered_read_buffer_release_slice(
        storage_buffered_channel_t *storage_buffered_channel);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_BUFFERED_H
