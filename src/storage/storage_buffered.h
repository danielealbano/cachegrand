#ifndef CACHEGRAND_STORAGE_BUFFERED_H
#define CACHEGRAND_STORAGE_BUFFERED_H

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_BUFFERED_H
