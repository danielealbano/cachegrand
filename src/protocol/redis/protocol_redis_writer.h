
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

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PROTOCOL_REDIS_WRITER_H
