#ifndef CACHEGRAND_PROTOCOL_REDIS_H
#define CACHEGRAND_PROTOCOL_REDIS_H

#ifdef __cplusplus
extern "C" {
#endif

enum protocol_redis_types {
    PROTOCOL_REDIS_TYPE_SIMPLE_STRING = '+',
    PROTOCOL_REDIS_TYPE_BLOB_STRING = '$',
    PROTOCOL_REDIS_TYPE_VERBATIM_STRING = '=',
    PROTOCOL_REDIS_TYPE_NUMBER = ':',
    PROTOCOL_REDIS_TYPE_DOUBLE = ',',
    PROTOCOL_REDIS_TYPE_BIG_NUMBER = '(',
    PROTOCOL_REDIS_TYPE_NULL = '_',
    PROTOCOL_REDIS_TYPE_BOOLEAN = '#',
    PROTOCOL_REDIS_TYPE_ARRAY = '*',
    PROTOCOL_REDIS_TYPE_MAP = '%',
    PROTOCOL_REDIS_TYPE_SET = '~',
    PROTOCOL_REDIS_TYPE_ATTRIBUTE = '|',
    PROTOCOL_REDIS_TYPE_PUSH = '>',

    PROTOCOL_REDIS_TYPE_SIMPLE_ERROR = '-',
    PROTOCOL_REDIS_TYPE_BLOB_ERROR = '!',
};
typedef enum protocol_redis_types protocol_redis_types_t;

enum protocol_redis_resp_version {
    PROTOCOL_REDIS_RESP_VERSION_2,
    PROTOCOL_REDIS_RESP_VERSION_3
};
typedef enum protocol_redis_resp_version protocol_redis_resp_version_t;

enum protocol_redis_reader_protocol_types {
    PROTOCOL_REDIS_READER_PROTOCOL_TYPE_INLINE,
    PROTOCOL_REDIS_READER_PROTOCOL_TYPE_RESP
};
typedef enum protocol_redis_reader_protocol_types protocol_redis_reader_protocol_types_t;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PROTOCOL_REDIS_H
