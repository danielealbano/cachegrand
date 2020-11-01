#ifndef CACHEGRAND_PROTOCOL_RESP3_H
#define CACHEGRAND_PROTOCOL_RESP3_H

#ifdef __cplusplus
extern "C" {
#endif

#define PROTOCOL_RESP3_STACK_DEPTH_MAX   16


typedef enum protocol_resp3_status protocol_resp3_status_t;
enum protocol_resp3_status {
    PROTOCOL_RESP3_STATUS_WAITING_TYPE,
    PROTOCOL_RESP3_STATUS_WAITING_LENGTH,
    PROTOCOL_RESP3_STATUS_MESSAGE_READ
};

struct protocol_resp3_state {

};


typedef enum protocol_resp3_element_type protocol_resp3_element_type_t;
enum protocol_resp3_element_type {
    // Simple types
    PROTOCOL_RESP3_ELEMENT_TYPE_NULL = '_',
    PROTOCOL_RESP3_ELEMENT_TYPE_BLOB_STRING = '$',
    PROTOCOL_RESP3_ELEMENT_TYPE_BLOB_ERROR = '!',
    PROTOCOL_RESP3_ELEMENT_TYPE_VERBATIM_STRING = '=',
    PROTOCOL_RESP3_ELEMENT_TYPE_SIMPLE_STRING = '+',
    PROTOCOL_RESP3_ELEMENT_TYPE_SIMPLE_ERROR = '-',
    PROTOCOL_RESP3_ELEMENT_TYPE_BOOLEAN = '#',
    PROTOCOL_RESP3_ELEMENT_TYPE_NUMBER = ':',
    PROTOCOL_RESP3_ELEMENT_TYPE_FLOAT = ',',
    PROTOCOL_RESP3_ELEMENT_TYPE_BIG_NUMBER = '(',

    // Aggregated types
    PROTOCOL_RESP3_ELEMENT_TYPE_ARRAY = '*',
    PROTOCOL_RESP3_ELEMENT_TYPE_MAP = '%',
    PROTOCOL_RESP3_ELEMENT_TYPE_SETS = '~',
    PROTOCOL_RESP3_ELEMENT_TYPE_ATTRIBUTES = '|',

    // Stream types
    // CURRENTLY NOT SUPPORTED
};

typedef struct protocol_resp3_element protocol_resp3_element_t;
struct protocol_resp3_element {
    protocol_resp3_element_type_t type;
    uint16_t depth;

    union {
        struct {
            
        } string;
    };
};







typedef struct protocol_resp3_connection protocol_resp3_connection_t;
struct protocol_resp3_connection {
    uint8_t state_stack_index;

    struct {

    } stack[NETWORK_PROTOCOL_REDIS_RESP3_PARSER_STATE_STACK_DEPTH];

    union {
        struct {
            // nop
        } waiting_type;

        struct {
            // nop
        } waiting_length;

        struct {
            // nop
        } waiting_params;
    } state_stack[NETWORK_PROTOCOL_REDIS_RESP3_PARSER_STATE_STACK_DEPTH];

};

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PROTOCOL_RESP3_H
