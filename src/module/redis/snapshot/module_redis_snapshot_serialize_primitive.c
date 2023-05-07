/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <liblzf/lzf.h>
#include <assert.h>

#include "misc.h"
#include "xalloc.h"
#include "module_redis_snapshot.h"

#include "module_redis_snapshot_serialize_primitive.h"

size_t module_redis_snapshot_serialize_primitive_encode_length_required_buffer_space(
        uint64_t length) {
    size_t required_buffer_space;

    // Depending on the number, calculate the required length
    if (length <= 63) {
        required_buffer_space = 1;
    } else if (length <= 16383) {
        required_buffer_space = 2;
    } else if (length <= UINT32_MAX) {
        required_buffer_space = 5;
    } else {
        required_buffer_space = 9;
    }

    return required_buffer_space;
}

bool module_redis_snapshot_serialize_primitive_can_encode_string_int(
        char *string,
        size_t string_length,
        int64_t *string_integer_out) {
    bool result = true;
    char *string_end = NULL;

    // Duplicate the string to ensure that the string has a null terminator
    char *string_dup = xalloc_alloc(string_length + 1);
    memcpy(string_dup, string, string_length);
    string_dup[string_length] = '\0';

    // Convert the string to an integer
    if (string_dup[0] == '-') {
        // If the string starts with a minus sign, it is a signed integer so no need to check if it's greater
        // than INT64_MAX
        int64_t string_integer_signed = strtoll(string_dup, &string_end, 10);
        *string_integer_out = string_integer_signed;
    } else {
        // The string can an unsigned or a signed integer. If it's an unsigned integer, it can be greater than
        // INT64_MAX so we need to check that it's not greater than INT64_MAX.
        uint64_t string_integer_unsigned = strtoull(string_dup, &string_end, 10);
        *string_integer_out = (int64_t)string_integer_unsigned;

        if (string_integer_unsigned > *string_integer_out) {
            result = false;
            goto end;
        }
    }

    // Check the string was converted successfully
    if (string_end == string_dup || *string_end != '\0') {
        result = false;
        goto end;
    } else {
        // Redis only allows strings that contain numbers between INT32_MIN and INT32_MAX to be encoded as integers
        // so we need to check that the string is within that range.
        // The check is performed here for future compatibility in case it will be necessary to support also strings
        // that contain numbers between INT64_MIN and INT64_MAX.
        if (*string_integer_out < INT32_MIN || *string_integer_out > INT32_MAX) {
            result = false;
            goto end;
        }
    }

end:
    // Free the duplicated string
    xalloc_free(string_dup);

    return result;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_header(
        module_redis_snapshot_header_t *header,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    char magic_buffer[] = "REDIS";
    char version_buffer[5] = { 0 };
    *buffer_offset_out = buffer_offset;
    module_redis_snapshot_serialize_primitive_result_t result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK;

    if (*buffer_offset_out + 9 > buffer_size) {
        result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
        goto end;
    }

    // Prepare the version string
    snprintf(version_buffer, sizeof(version_buffer), "%04d", header->version);

    // Copy the magic string, skipping the null terminator
    memcpy(buffer + *buffer_offset_out, magic_buffer, sizeof(magic_buffer) - 1);
    *buffer_offset_out += 5;

    // Copy the version string, skipping the null terminator
    memcpy(buffer + *buffer_offset_out, version_buffer, sizeof(version_buffer) - 1);
    *buffer_offset_out += 4;

end:
    return result;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_length_up_to_uint63(
        uint64_t length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    *buffer_offset_out = buffer_offset;
    size_t required_buffer_space = module_redis_snapshot_serialize_primitive_encode_length_required_buffer_space(
            length);

    if (unlikely(length > 63)) {
        return MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_NUMBER_TOO_BIG;
    }

    if (unlikely(*buffer_offset_out + required_buffer_space > buffer_size)) {
        return MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
    }

    // Numbers up to 63 included are encoded in a single byte, the first two bits are set to 0
    buffer[*buffer_offset_out] = length & 0x3F;
    (*buffer_offset_out)++;

    return MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_length_up_to_uint16383(
        uint64_t length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    *buffer_offset_out = buffer_offset;
    size_t required_buffer_space = module_redis_snapshot_serialize_primitive_encode_length_required_buffer_space(
            length);

    if (unlikely(length > 16383)) {
        return MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_NUMBER_TOO_BIG;
    }

    if (unlikely(*buffer_offset_out + required_buffer_space > buffer_size)) {
        return MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
    }

    // Numbers up to 16383 included are encoded in two bytes, the first two bits of the first byte are set to 01 and
    // the first remaining 6 bits are used to encode the upper 6 bits of the number
    buffer[*buffer_offset_out] = ((length >> 8) & 0x3F) | 0x40;
    (*buffer_offset_out)++;
    buffer[*buffer_offset_out] = length & 0xFF;
    (*buffer_offset_out)++;

    return MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_length_up_to_uint32b(
        uint64_t length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    *buffer_offset_out = buffer_offset;
    size_t required_buffer_space = module_redis_snapshot_serialize_primitive_encode_length_required_buffer_space(
            length);

    if (unlikely(length > UINT32_MAX)) {
        return MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_NUMBER_TOO_BIG;
    }

    if (unlikely(*buffer_offset_out + required_buffer_space > buffer_size)) {
        return MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
    }

    // Numbers up to UINT32_MAX included are encoded in five bytes, the first two bits of the first byte are set to
    // 10 and the remaining 6 bits of the first byte are discarded. The number is encoded using big endian encoding
    // (most significant byte first) in the remaining four bytes
    buffer[*buffer_offset_out] = 0x80;
    (*buffer_offset_out)++;
    uint32_t length_32 = int32_hton(length);
    memcpy(buffer + *buffer_offset_out, &length_32, sizeof(length_32));
    *buffer_offset_out += sizeof(length_32);

    return MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_length_up_to_uint64b(
        uint64_t length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    *buffer_offset_out = buffer_offset;
    size_t required_buffer_space = module_redis_snapshot_serialize_primitive_encode_length_required_buffer_space(
            length);

    if (unlikely(*buffer_offset_out + required_buffer_space > buffer_size)) {
        return MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
    }

    // Numbers greater than UINT32_MAX are encoded in nine bytes, the first two bits of the first byte are set to
    // 11 and the remaining 6 bits of the first byte are discarded. The number is encoded using big endian encoding
    // (most significant byte first) in the remaining eight bytes
    buffer[*buffer_offset_out] = 0x81;
    (*buffer_offset_out)++;
    uint64_t length_64 = int64_hton(length);
    memcpy(buffer + *buffer_offset_out, &length_64, sizeof(length_64));
    *buffer_offset_out += sizeof(length_64);

    return MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_length(
        uint64_t length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    if (length <= 63) {
        return module_redis_snapshot_serialize_primitive_encode_length_up_to_uint63(
                length,
                buffer,
                buffer_size,
                buffer_offset,
                buffer_offset_out);
    } else if (length <= 16383) {
        return module_redis_snapshot_serialize_primitive_encode_length_up_to_uint16383(
                length,
                buffer,
                buffer_size,
                buffer_offset,
                buffer_offset_out);
    } else if (length <= UINT32_MAX) {
        return module_redis_snapshot_serialize_primitive_encode_length_up_to_uint32b(
                length,
                buffer,
                buffer_size,
                buffer_offset,
                buffer_offset_out);
    } else {
        return module_redis_snapshot_serialize_primitive_encode_length_up_to_uint64b(
                length,
                buffer,
                buffer_size,
                buffer_offset,
                buffer_offset_out);
    }
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_key(
        char *string,
        size_t string_length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    assert(string_length > 0);

    *buffer_offset_out = buffer_offset;
    module_redis_snapshot_serialize_primitive_result_t result;
    size_t required_buffer_space =
            module_redis_snapshot_serialize_primitive_encode_length_required_buffer_space(string_length) +
            string_length;

    // Check if the buffer is big enough
    if (*buffer_offset_out + required_buffer_space > buffer_size) {
        result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
        goto end;
    }

    // Encode the length
    if ((result = module_redis_snapshot_serialize_primitive_encode_length(
            string_length,
            buffer,
            buffer_size,
            *buffer_offset_out,
            buffer_offset_out)) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        goto end;
    }

    // Copy the string
    memcpy(buffer + *buffer_offset_out, string, string_length);
    *buffer_offset_out += string_length;

    end:
    return result;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_string_length(
        size_t string_length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    return module_redis_snapshot_serialize_primitive_encode_length(
            string_length,
            buffer,
            buffer_size,
            buffer_offset,
            buffer_offset_out);
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_string_data_plain(
        char *string,
        size_t string_length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    *buffer_offset_out = buffer_offset;
    module_redis_snapshot_serialize_primitive_result_t result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK;

    // Check if the buffer is big enough
    if (*buffer_offset_out + string_length > buffer_size) {
        result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
        goto end;
    }

    // Copy the string
    memcpy(buffer + *buffer_offset_out, string, string_length);
    *buffer_offset_out += string_length;

    end:
    return result;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_small_string_int(
        int64_t string_integer,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    *buffer_offset_out = buffer_offset;
    module_redis_snapshot_serialize_primitive_result_t result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK;

    if (string_integer >= INT8_MIN && string_integer <= INT8_MAX) {
        // Numbers between INT8_MIN and INT8_MAX are encoded in two bytes, the first byte is set to 0xC0 and the
        // second byte is set to the number
        if (*buffer_offset_out + 2 > buffer_size) {
            result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
            goto end;
        }

        // Set the type of encoding
        buffer[*buffer_offset_out] = 0xC0 | 0x00;
        (*buffer_offset_out)++;

        // Store the number
        buffer[*buffer_offset_out] = string_integer & 0xFF;
        (*buffer_offset_out)++;
    } else if (string_integer >= INT16_MIN && string_integer <= INT16_MAX) {
        // Numbers between INT16_MIN and INT16_MAX are encoded in three bytes, the first byte is set to 0xC0 | 0x01 and
        // the second and third bytes are set to the number
        if (*buffer_offset_out + 3 > buffer_size) {
            result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
            goto end;
        }

        // Set the type of encoding
        buffer[*buffer_offset_out] = 0xC0 | 0x01;
        (*buffer_offset_out)++;

        // Store the number in little endian format
        buffer[*buffer_offset_out] = string_integer & 0xFF;
        (*buffer_offset_out)++;
        buffer[*buffer_offset_out] = (string_integer >> 8) & 0xFF;
        (*buffer_offset_out)++;
    } else if (string_integer >= INT32_MIN && string_integer <= INT32_MAX) {
        // Numbers between INT32_MIN and INT32_MAX are encoded in five bytes, the first byte is set to 0xC0 | 0x02 and
        // the second, third, fourth and fifth bytes are set to the number.
        // The number is stored in little endian format
        if (*buffer_offset_out + 5 > buffer_size) {
            result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
            goto end;
        }

        // Set the type of encoding
        buffer[*buffer_offset_out] = 0xC0 | 0x02;
        (*buffer_offset_out)++;

        // Store the number in little endian format
        buffer[*buffer_offset_out] = string_integer & 0xFF;
        (*buffer_offset_out)++;
        buffer[*buffer_offset_out] = (string_integer >> 8) & 0xFF;
        (*buffer_offset_out)++;
        buffer[*buffer_offset_out] = (string_integer >> 16) & 0xFF;
        (*buffer_offset_out)++;
        buffer[*buffer_offset_out] = (string_integer >> 24) & 0xFF;
        (*buffer_offset_out)++;
    } else {
        result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_INVALID_INTEGER;
    }

    end:
    return result;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_small_string_lzf(
        char *string,
        size_t string_length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    *buffer_offset_out = buffer_offset;
    module_redis_snapshot_serialize_primitive_result_t result;

    if (unlikely(string_length >= 64 * 1024)) {
        return MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_TOO_LARGE;
    }

    // Calculate the maximum required buffer space
    size_t max_required_buffer_space = 1 + 5 + 5 + LZF_MAX_COMPRESSED_SIZE(string_length);

    // Check if the buffer is big enough
    if (*buffer_offset_out + max_required_buffer_space > buffer_size) {
        result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
        goto end;
    }

    // Try to compress the data
    size_t compressed_string_length = lzf_compress(
            string,
            string_length,
            buffer + *buffer_offset_out,
            buffer_size - *buffer_offset_out);

    // Check if the compression was successful, if not, return an error
    if (compressed_string_length == 0) {
        result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_COMPRESSION_FAILED;
        goto end;
    }

    // If the compression ratio is too low (the compressed string is greater than the raw string), don't use compression
    if (compressed_string_length > string_length) {
        result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_COMPRESSION_RATIO_TOO_LOW;
        goto end;
    }

    // Set the type of encoding
    buffer[*buffer_offset_out] = 0xC0 | 0x03;
    (*buffer_offset_out)++;

    // Encode the length of the string
    if ((result = module_redis_snapshot_serialize_primitive_encode_length_up_to_uint32b(
            compressed_string_length,
            buffer,
            buffer_size,
            *buffer_offset_out,
            buffer_offset_out)) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        goto end;
    }

    // Encode the length of the string
    if ((result = module_redis_snapshot_serialize_primitive_encode_length_up_to_uint32b(
            string_length,
            buffer,
            buffer_size,
            *buffer_offset_out,
            buffer_offset_out)) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        goto end;
    }

    // Update the buffer offset
    *buffer_offset_out += compressed_string_length;

end:
    return result;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_small_string(
        char *string,
        size_t string_length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    module_redis_snapshot_serialize_primitive_result_t result;

    if (unlikely(string_length >= 64 * 1024)) {
        return MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_TOO_LARGE;
    }

    int64_t string_integer = 0;
    if (module_redis_snapshot_serialize_primitive_can_encode_string_int(
            string,
            string_length,
            &string_integer)) {
        return module_redis_snapshot_serialize_primitive_encode_small_string_int(
                string_integer,
                buffer,
                buffer_size,
                buffer_offset,
                buffer_offset_out);
    }

    // cachegrand stores strings that are bigger than 64kb in different memory locations but the LZF library doesn't
    // support compressing streams therefore the compression can be applied only if the string is smaller than 64kb.
    // Also, because the LZF isn't capable of compressing small blocks of data, if the string is shorter than 32 bytes
    // the compression is not applied.
    if (string_length > 32) {
        result = module_redis_snapshot_serialize_primitive_encode_small_string_lzf(
                string,
                string_length,
                buffer,
                buffer_size,
                buffer_offset,
                buffer_offset_out);

        if (result != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_COMPRESSION_RATIO_TOO_LOW) {
            return result;
        }
    }

    // If it gets here, it means that the compression was not applied, so encode the string as a plain string
    if ((result = module_redis_snapshot_serialize_primitive_encode_string_length(
            string_length,
            buffer,
            buffer_size,
            buffer_offset,
            buffer_offset_out)) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        return result;
    }

    return module_redis_snapshot_serialize_primitive_encode_string_data_plain(
            string,
            string_length,
            buffer,
            buffer_size,
            *buffer_offset_out,
            buffer_offset_out);
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_opcode_aux(
        char *key,
        size_t key_length,
        char *value,
        size_t value_length,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    *buffer_offset_out = buffer_offset;
    module_redis_snapshot_serialize_primitive_result_t result;

    if (*buffer_offset_out + 1 + key_length + value_length > buffer_size) {
        result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
        goto end;
    }

    buffer[*buffer_offset_out] = MODULE_REDIS_SNAPSHOT_OPCODE_AUX;
    (*buffer_offset_out)++;

    if ((result = module_redis_snapshot_serialize_primitive_encode_small_string(
            key,
            key_length,
            buffer,
            buffer_size,
            *buffer_offset_out,
            buffer_offset_out)) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        goto end;
    }

    if ((result = module_redis_snapshot_serialize_primitive_encode_small_string(
            value,
            value_length,
            buffer,
            buffer_size,
            *buffer_offset_out,
            buffer_offset_out)) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        goto end;
    }

end:
    return result;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_opcode_db_number(
        uint64_t db_number,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    *buffer_offset_out = buffer_offset;
    module_redis_snapshot_serialize_primitive_result_t result;

    if (*buffer_offset_out + 2 > buffer_size) {
        result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
        goto end;
    }

    buffer[*buffer_offset_out] = MODULE_REDIS_SNAPSHOT_OPCODE_DB_NUMBER;
    (*buffer_offset_out)++;

    result = module_redis_snapshot_serialize_primitive_encode_length(
            db_number,
            buffer,
            buffer_size,
            *buffer_offset_out,
            buffer_offset_out);

end:
    return result;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_opcode_expire_time_s(
        uint32_t expire_time_s,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    *buffer_offset_out = buffer_offset;
    module_redis_snapshot_serialize_primitive_result_t result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK;

    if (*buffer_offset_out + 5 > buffer_size) {
        result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
        goto end;
    }

    buffer[*buffer_offset_out] = MODULE_REDIS_SNAPSHOT_OPCODE_EXPIRE_TIME;
    (*buffer_offset_out)++;

    uint32_t expire_time_s_be = int32_htole(expire_time_s);
    memcpy(buffer + *buffer_offset_out, &expire_time_s_be, sizeof(expire_time_s_be));
    *buffer_offset_out += sizeof(expire_time_s_be);

end:
    return result;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_opcode_expire_time_ms(
        uint64_t expire_time_ms,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    *buffer_offset_out = buffer_offset;
    module_redis_snapshot_serialize_primitive_result_t result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK;

    if (*buffer_offset_out + 8 > buffer_size) {
        result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
        goto end;
    }

    buffer[*buffer_offset_out] = MODULE_REDIS_SNAPSHOT_OPCODE_EXPIRE_TIME_MS;
    (*buffer_offset_out)++;

    uint64_t expire_time_ms_be = int64_htole(expire_time_ms);
    memcpy(buffer + *buffer_offset_out, &expire_time_ms_be, sizeof(expire_time_ms_be));
    *buffer_offset_out += sizeof(expire_time_ms_be);

end:
    return result;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_opcode_value_type(
        module_redis_snapshot_value_type_t value_type,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    *buffer_offset_out = buffer_offset;
    module_redis_snapshot_serialize_primitive_result_t result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK;

    if (*buffer_offset_out + 1 > buffer_size) {
        result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
        goto end;
    }

    assert(module_redis_snapshot_is_value_type_valid(value_type));
    assert(module_redis_snapshot_is_value_type_supported(value_type));

    buffer[*buffer_offset_out] = value_type;
    (*buffer_offset_out)++;

end:
    return result;
}

module_redis_snapshot_serialize_primitive_result_t module_redis_snapshot_serialize_primitive_encode_opcode_eof(
        uint64_t checksum,
        uint8_t *buffer,
        size_t buffer_size,
        size_t buffer_offset,
        size_t *buffer_offset_out) {
    *buffer_offset_out = buffer_offset;
    module_redis_snapshot_serialize_primitive_result_t result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK;

    if (*buffer_offset_out + 9 > buffer_size) {
        result = MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW;
        goto end;
    }

    buffer[*buffer_offset_out] = MODULE_REDIS_SNAPSHOT_OPCODE_EOF;
    (*buffer_offset_out)++;

    // Write the checksum
    uint64_t checksum_le = int64_htole(checksum);
    memcpy(buffer + *buffer_offset_out, &checksum_le, sizeof(checksum_le));
    *buffer_offset_out += sizeof(checksum_le);

end:
    return result;
}
