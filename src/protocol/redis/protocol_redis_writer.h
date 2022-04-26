
#ifndef CACHEGRAND_PROTOCOL_REDIS_WRITER_H
#define CACHEGRAND_PROTOCOL_REDIS_WRITER_H

#ifdef __cplusplus
extern "C" {
#endif

#define PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_WRITE_FUNC_VA_ARGS(...) , ##__VA_ARGS__

#define PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER(BUFFER, BUFFER_LENGTH, BUFFER_START, WRITE_FUNC, ...) { \
        if ((BUFFER = WRITE_FUNC( \
            BUFFER_START, BUFFER_LENGTH PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_WRITE_FUNC_VA_ARGS(__VA_ARGS__))) == NULL) { \
                return NULL; \
        } \
        \
        BUFFER_LENGTH -= BUFFER - BUFFER_START; \
        BUFFER_START = BUFFER; \
    }

#define PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS(WRITE_FUNC, ...) { \
        PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER( \
            buffer, \
            buffer_length, \
            buffer_start, \
            WRITE_FUNC, \
            __VA_ARGS__) \
    }

#define PROTOCOL_REDIS_WRITER_WRITE_FUNC_ARGUMENTS(...) , ## __VA_ARGS__

#define PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(NAME, ARGUMENTS) \
    char* protocol_redis_writer_write_##NAME( \
        char* buffer, \
        size_t buffer_length \
        PROTOCOL_REDIS_WRITER_WRITE_FUNC_ARGUMENTS ARGUMENTS)

#define PROTOCOL_REDIS_WRITER_WRITE_FUNC_WRAPPER(TYPE, NAME, ARGUMENTS, ...) \
    PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(NAME, ARGUMENTS) { \
        char* buffer_start = buffer; \
        \
        PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS( \
            protocol_redis_writer_write_argument_type, \
            TYPE) \
        \
        __VA_ARGS__ \
        \
        PROTOCOL_REDIS_WRITER_WRITE_ARGUMENT_WRAPPER_COMMON_VARS( \
            protocol_redis_writer_write_argument_eol) \
        \
        return buffer; \
    }

PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(null, ());
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(boolean, (bool is_true));
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(blob_string, (char* string, int string_length));
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(blob_string_null, ());
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(blob_error, (char* string, int string_length));
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(simple_string, (char* string, int string_length));
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(simple_error, (char* string, int string_length));
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(simple_error_printf, (char* string, ...));
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(number, (long number));
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(double, (double number));
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(big_number, (char* bignumber, int bignumber_length));
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(array, (uint32_t array_count));
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(map, (uint32_t items_count));
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(set, (uint32_t set_count));
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(attribute, (uint32_t attributes_count));
PROTOCOL_REDIS_WRITER_WRITE_FUNC_NAME(push, (uint32_t messages_count));

bool protocol_redis_writer_enough_space_in_buffer(
        size_t length,
        size_t requested_size);

unsigned protocol_redis_writer_uint64_str_length(
        uint64_t number);

unsigned protocol_redis_writer_int64_str_length(
        int64_t number);

char* protocol_redis_writer_uint64_to_str(
        uint64_t number,
        size_t number_str_length,
        char* buffer,
        size_t buffer_length);

char* protocol_redis_writer_int64_to_str(
        int64_t number,
        size_t number_str_length,
        char* buffer,
        size_t buffer_length);

unsigned protocol_redis_writer_double_str_length(
        double number,
        int round_to_digits,
        size_t* integer_part_length,
        size_t* fraction_part_length);

char* protocol_redis_writer_double_to_str(
        double number,
        int round_to_digits,
        size_t integer_part_length,
        size_t fraction_part_length,
        char* buffer,
        size_t buffer_length);

char* protocol_redis_writer_write_argument_type(
        char* buffer,
        size_t buffer_length,
        protocol_redis_types_t type);

char* protocol_redis_writer_write_argument_boolean(
        char* buffer,
        size_t buffer_length,
        bool is_true);

char* protocol_redis_writer_write_argument_number(
        char* buffer,
        size_t buffer_length,
        int64_t number);

char* protocol_redis_writer_write_argument_double(
        char* buffer,
        size_t buffer_length,
        double number);

char* protocol_redis_writer_write_argument_string(
        char* buffer,
        size_t buffer_length,
        char* string,
        size_t string_length);

char* protocol_redis_writer_write_argument_string_printf(
        char* buffer,
        size_t buffer_length,
        char* string,
        va_list args);

char* protocol_redis_writer_write_argument_eol(
        char* buffer,
        size_t buffer_length);

char* protocol_redis_writer_write_argument_blob_start(
        char* buffer,
        size_t buffer_length,
        bool is_error,
        int64_t string_length);

char* protocol_redis_writer_write_argument_blob_end(
        char* buffer,
        size_t buffer_length);

char* protocol_redis_writer_write_argument_blob(
        char* buffer,
        size_t buffer_length,
        bool is_error,
        char* string,
        int64_t string_length);

char* protocol_redis_writer_write_argument_simple(
        char* buffer,
        size_t buffer_length,
        bool is_error,
        char* string,
        int64_t string_length);

char* protocol_redis_writer_write_argument_simple_printf(
        char* buffer,
        size_t buffer_length,
        bool is_error,
        char* string,
        va_list args);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PROTOCOL_REDIS_WRITER_H
