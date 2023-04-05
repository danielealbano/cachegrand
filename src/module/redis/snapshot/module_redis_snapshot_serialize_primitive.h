#ifndef CACHEGRAND_MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_H
#define CACHEGRAND_MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_H

#ifdef __cplusplus
extern "C" {
#endif

enum module_redis_snapshot_serialize_primitive_result {
    MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK,
    MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW,
    MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_INVALID_INTEGER,
    MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_COMPRESSION_FAILED,
    MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_COMPRESSION_RATIO_TOO_LOW,
    MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_INVALID_VALUE_TYPE,
};
typedef enum module_redis_snapshot_serialize_primitive_result module_redis_snapshot_serialize_primitive_result_t;

/**
 * The LZF_MAX_COMPRESSED_SIZE macro below is imported from from https://github.com/nemequ/liblzf/blob/master/lzf.h,
 * which is licensed under the following terms
 *
 * Copyright (c) 2000-2008 Marc Alexander Lehmann <schmorp@schmorp.de>
 *
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 *
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License ("GPL") version 2 or any later version,
 * in which case the provisions of the GPL are applicable instead of
 * the above. If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the BSD license, indicate your decision
 * by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL. If you do not delete the
 * provisions above, a recipient may use your version of this file under
 * either the BSD or the GPL.
 */
/**
 * The maximum out_len that needs to be allocated to make sure
 * any input data can be compressed without overflowing the output
 * buffer, i.e. maximum out_len = LZF_MAX_COMPRESSED_SIZE (in_len).
 * This is useful if you don't want to bother with the case of
 * incompressible data and just want to provide a buffer that is
 * guaranteeed to be big enough.
 * This macro can be used at preprocessing time.
 */
#define LZF_MAX_COMPRESSED_SIZE(n) ((((n) * 33) >> 5 ) + 1)

size_t module_redis_snapshot_serialize_primitive_encode_length_required_buffer_space(
        uint64_t length);

bool module_redis_snapshot_serialize_primitive_can_encode_string_int(
        char *string,
        size_t string_length,
        int64_t *string_integer_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_header(
        module_redis_snapshot_header_t *header,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_length(
        uint64_t length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_key(
        char *string,
        size_t string_length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_string_length(
        size_t string_length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_string_data_plain(
        char *string,
        size_t string_length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_small_string_int(
        int64_t string_integer,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_small_string_lzf(
        char *string,
        size_t string_length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_small_string(
        char *string,
        size_t string_length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_opcode_db_number(
        uint64_t db_number,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_opcode_aux(
        char *key,
        size_t key_length,
        char *value,
        size_t value_length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_opcode_expire_time_s(
        uint32_t expire_time_s,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_opcode_expire_time_ms(
        uint64_t expire_time_ms,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_opcode_value_type(
        module_snapshot_value_type_t value_type,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_opcode_eof(
        uint64_t checksum,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_H
