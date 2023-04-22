/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "exttypes.h"
#include "misc.h"
#include "log/log.h"
#include "protocol_redis.h"

#include "protocol_redis_writer.h"

#define TAG "protocol_redis_writer"

bool protocol_redis_writer_enough_space_in_buffer(
        size_t length,
        size_t requested_size) {
    return requested_size < length;
}

unsigned protocol_redis_writer_uint64_str_length(
        uint64_t number) {
    static uint8_t maxdigits[65] = {
            1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 6, 6, 6,
            7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11,
            12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15, 15, 15,
            16, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 19, 20,
    };

    static uint64_t powers[] = {
            0U, 1U, 10U, 100U, 1000U, 10000U, 100000U, 1000000U, 10000000U,
            100000000U, 1000000000U, 10000000000U, 100000000000U,
            1000000000000U, 10000000000000U, 100000000000000U,
            1000000000000000U, 10000000000000000U, 100000000000000000U,
            1000000000000000000U, 10000000000000000000U,
    };

    if (number == 0) {
        return 1;
    }

    unsigned bits = sizeof(number) * CHAR_BIT - __builtin_clzll(number);
    unsigned digits = maxdigits[bits];

    if (number < powers[digits]) {
        --digits;
    }

    return digits;
}

unsigned protocol_redis_writer_int64_str_length(
        int64_t number) {
    int add_minus_sign = number < 0U ? 1 : 0;
    if (add_minus_sign) {
        number *= -1;
    }

    return protocol_redis_writer_uint64_str_length(number) + add_minus_sign;
}


char* protocol_redis_writer_uint64_to_str(
        uint64_t number,
        size_t number_str_length,
        char* buffer,
        size_t buffer_length) {
    char* buffer_end_ptr = buffer + number_str_length;

    if (number_str_length == 0 || number_str_length > buffer_length) {
        return NULL;
    }

    do {
        buffer[number_str_length - 1] = (char)((number % 10U) + '0');
    } while(number_str_length-- > 0 && (number /= 10U) > 0);

    if (number_str_length == 0 && number > 0) {
        return NULL;
    }

    return buffer_end_ptr;
}

char* protocol_redis_writer_int64_to_str(
        int64_t number,
        size_t number_str_length,
        char* buffer,
        size_t buffer_length) {
    if (number_str_length == 0 || number_str_length > buffer_length) {
        return NULL;
    }

    if (number < 0) {
        *buffer++ = '-';
        buffer_length--;
        number_str_length--;
        number = number * -1;
    }

    return protocol_redis_writer_uint64_to_str(number, number_str_length, buffer, buffer_length);
}

unsigned protocol_redis_writer_double_str_length(
        double number,
        int round_to_digits,
        size_t* integer_part_length,
        size_t* fraction_part_length) {
    *integer_part_length = protocol_redis_writer_int64_str_length((int64_t) number);
    *fraction_part_length = 0;

    if (number < 0) {
        number *= -1;
    }

    number -= (int)number;

    while(number > 0 && round_to_digits-- > 0) {
        (*fraction_part_length)++;

        number *= 10;
        number -= (int)number;
    }

    // Take into account the dot
    if (*fraction_part_length > 0) {
        (*fraction_part_length)++;
    }

    return *integer_part_length + *fraction_part_length;
}

char* protocol_redis_writer_double_to_str(
        double number,
        int round_to_digits,
        size_t integer_part_length,
        size_t fraction_part_length,
        char* buffer,
        size_t buffer_length) {
    if (integer_part_length + fraction_part_length > buffer_length) {
        return NULL;
    }

    if (integer_part_length > 0) {
        buffer = protocol_redis_writer_int64_to_str((int)number, integer_part_length, buffer, buffer_length);
    }

    if (fraction_part_length > 0) {
        *buffer++ = '.';

        if (number < 0) {
            number *= -1;
            buffer_length--;
        }

        number -= (int)number;

        // TODO: not perfect implementation, there is no rounding and after 12 digits it starts to differ from sprintf output

        while(number > 0 && round_to_digits-- > 0) {
            number *= 10;
            int digit = (int)number;
            *buffer++ = (char)(digit + '0');
            number -= digit;
        }
    }

    return buffer;
}

char* protocol_redis_writer_write_argument_type(
        char* buffer,
        size_t buffer_length,
        protocol_redis_types_t type) {
    if (!protocol_redis_writer_enough_space_in_buffer(buffer_length, 1)) {
        return NULL;
    }

    *buffer++ = type;

    return buffer;
}

char* protocol_redis_writer_write_argument_boolean(
        char* buffer,
        size_t buffer_length,
        bool is_true) {
    if (!protocol_redis_writer_enough_space_in_buffer(buffer_length, 1)) {
        return NULL;
    }

    *buffer++ = is_true ? 't' : 'f';

    return buffer;
}

char* protocol_redis_writer_write_argument_number(
        char* buffer,
        size_t buffer_length,
        int64_t number) {
    size_t number_size = protocol_redis_writer_int64_str_length(number);

    if (!protocol_redis_writer_enough_space_in_buffer(buffer_length, number_size)) {
        return NULL;
    }

    return protocol_redis_writer_int64_to_str(number, number_size, buffer, buffer_length);
}

char* protocol_redis_writer_write_argument_double(
        char* buffer,
        size_t buffer_length,
        double number) {
    size_t integer_part_length, fraction_part_length;
    size_t number_size = protocol_redis_writer_double_str_length(
            number,
            15,
            &integer_part_length,
            &fraction_part_length);

    if (!protocol_redis_writer_enough_space_in_buffer(buffer_length, number_size)) {
        return NULL;
    }

    return protocol_redis_writer_double_to_str(
            number,
            15,
            integer_part_length,
            fraction_part_length,
            buffer,
            buffer_length);
}

char* protocol_redis_writer_write_argument_string(
        char* buffer,
        size_t buffer_length,
        char* string,
        size_t string_length) {
    if (!protocol_redis_writer_enough_space_in_buffer(buffer_length, string_length)) {
        return NULL;
    }

    memcpy(buffer, string, string_length);

    return buffer + string_length;
}

char* protocol_redis_writer_write_argument_string_printf(
        char* buffer,
        size_t buffer_length,
        char* string,
        va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    ssize_t string_length = vsnprintf(NULL, 0, string, args_copy);
    va_end(args_copy);

    if (string_length < 0) {
        LOG_E(TAG, "Failed to calculate string length for format <%s> with the arguments", string);
        LOG_E_OS_ERROR(TAG);

        return NULL;
    }

    // We need to have access to an extra byte because vsnprintf adds the null terminator for the string even if we
    // don't need it. But in this way we avoid allocating extra memory just to copy the relevant part, the null byte
    // will get overwritten if necessary
    if (!protocol_redis_writer_enough_space_in_buffer(buffer_length, string_length + 1)) {
        return NULL;
    }

    if (vsnprintf(buffer, string_length + 1, string, args) < 0) {
        LOG_E(TAG, "Failed to format string <%s> with the arguments", string);
        LOG_E_OS_ERROR(TAG);

        return NULL;
    }

    return buffer + string_length;
}

char* protocol_redis_writer_write_argument_eol(
        char* buffer,
        size_t buffer_length) {
    if (!protocol_redis_writer_enough_space_in_buffer(buffer_length, 2)) {
        return NULL;
    }

    *buffer++ = '\r';
    *buffer++ = '\n';

    return buffer;
}

char* protocol_redis_writer_write_argument_blob_start(
        char* buffer,
        size_t buffer_length,
        bool is_error,
        int64_t string_length) {
    char* buffer_start = buffer;

    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_type,
        is_error ? PROTOCOL_REDIS_TYPE_BLOB_ERROR : PROTOCOL_REDIS_TYPE_BLOB_STRING)

    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
            protocol_redis_writer_write_argument_number,
            string_length)

    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_eol)

    return buffer;
}

char* protocol_redis_writer_write_argument_blob_end(
        char* buffer,
        size_t buffer_length) {
    char* buffer_start = buffer;

    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_eol)

    return buffer;
}

char* protocol_redis_writer_write_argument_blob(
        char* buffer,
        size_t buffer_length,
        bool is_error,
        char* string,
        int64_t string_length) {
    char* buffer_start = buffer;

    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_blob_start,
        is_error,
        string_length);

    if (string_length > -1) {
        PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
            protocol_redis_writer_write_argument_string,
            string,
            string_length);

        PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
            protocol_redis_writer_write_argument_blob_end)
    }

    return buffer;
}

char* protocol_redis_writer_write_argument_simple(
        char* buffer,
        size_t buffer_length,
        bool is_error,
        char* string,
        int64_t string_length) {
    char* buffer_start = buffer;

    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_type,
        is_error ? PROTOCOL_REDIS_TYPE_SIMPLE_ERROR : PROTOCOL_REDIS_TYPE_SIMPLE_STRING)

    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_string,
        string,
        string_length);

    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_eol)

    return buffer;
}

char* protocol_redis_writer_write_argument_simple_printf(
        char* buffer,
        size_t buffer_length,
        bool is_error,
        char* string,
        va_list args) {
    char *buffer_start = buffer;

    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
            protocol_redis_writer_write_argument_type,
            is_error ? PROTOCOL_REDIS_TYPE_SIMPLE_ERROR : PROTOCOL_REDIS_TYPE_SIMPLE_STRING)

    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
            protocol_redis_writer_write_argument_string_printf,
            string,
            args);

    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
            protocol_redis_writer_write_argument_eol)

    return buffer;
}

PROTOCOL_REDIS_WRITER_WRITE_FUNC_WRAPPER(PROTOCOL_REDIS_TYPE_NULL, null, (), {})

PROTOCOL_REDIS_WRITER_WRITE_FUNC_WRAPPER(PROTOCOL_REDIS_TYPE_BOOLEAN, boolean, (bool is_true), {
    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
            protocol_redis_writer_write_argument_boolean,
            is_true)
})

PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(blob_string, (char* string, int string_length)) {
    return protocol_redis_writer_write_argument_blob(buffer, buffer_length, false, string, string_length);
}

PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(blob_string_null, ()) {
    return protocol_redis_writer_write_argument_blob(buffer, buffer_length, false, NULL, -1);
}

PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(blob_error, (char* string, int string_length)) {
    return protocol_redis_writer_write_argument_blob(buffer, buffer_length, true, string, string_length);
}

PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(simple_string, (char* string, int string_length)) {
    return protocol_redis_writer_write_argument_simple(buffer, buffer_length, false, string, string_length);
}

PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(simple_error, (char* string, int string_length)) {
    return protocol_redis_writer_write_argument_simple(buffer, buffer_length, true, string, string_length);
}

PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(simple_error_printf, (char* string, ...)) {
    va_list args;
    va_start(args, string);
    char *res = protocol_redis_writer_write_argument_simple_printf(buffer, buffer_length, true, string, args);
    va_end(args);
    return res;
}

PROTOCOL_REDIS_WRITER_WRITE_FUNC_WRAPPER(PROTOCOL_REDIS_TYPE_NUMBER, number, (long number), {
    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_number,
        number)
})

PROTOCOL_REDIS_WRITER_WRITE_FUNC_WRAPPER(PROTOCOL_REDIS_TYPE_DOUBLE, double, (double number), {
    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_double,
        number)
})

PROTOCOL_REDIS_WRITER_WRITE_FUNC_WRAPPER(PROTOCOL_REDIS_TYPE_BIG_NUMBER, big_number, (char* bignumber, int bignumber_length), {
    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_number,
        bignumber_length)

    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_eol)

    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_string,
        bignumber,
        bignumber_length);
})

PROTOCOL_REDIS_WRITER_WRITE_FUNC_WRAPPER(PROTOCOL_REDIS_TYPE_ARRAY, array, (uint32_t array_count), {
    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_number,
        array_count)
})

PROTOCOL_REDIS_WRITER_WRITE_FUNC_WRAPPER(PROTOCOL_REDIS_TYPE_MAP, map, (uint32_t items_count), {
    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_number,
        items_count)
})

PROTOCOL_REDIS_WRITER_WRITE_FUNC_WRAPPER(PROTOCOL_REDIS_TYPE_SET, set, (uint32_t set_count), {
    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_number,
        set_count)
})

PROTOCOL_REDIS_WRITER_WRITE_FUNC_WRAPPER(PROTOCOL_REDIS_TYPE_ATTRIBUTE, attribute, (uint32_t attributes_count), {
    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_number,
        attributes_count)
})

PROTOCOL_REDIS_WRITER_WRITE_FUNC_WRAPPER(PROTOCOL_REDIS_TYPE_PUSH, push, (uint32_t messages_count), {
    PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(
        protocol_redis_writer_write_argument_number,
        messages_count)
})
