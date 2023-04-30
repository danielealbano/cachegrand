#ifndef CACHEGRAND_MODULE_H
#define CACHEGRAND_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct config_module config_module_t;
typedef struct network_channel network_channel_t;

typedef uint32_t module_id_t;
typedef bool (module_config_prepare_cb_t)(
        config_module_t *module);
typedef bool (module_config_validate_after_load_cb_t)(
        config_module_t *module);
typedef bool (module_worker_module_ctor_cb_t)(
        config_module_t *module);
typedef bool (module_worker_module_dtor_cb_t)(
        config_module_t *module);
typedef void (module_connection_accept_cb_t)(
        network_channel_t *network_channel);

struct module {
    module_id_t id;
    const char *name;
    module_config_prepare_cb_t *config_prepare;
    module_config_validate_after_load_cb_t *config_validate_after_load;
    module_worker_module_ctor_cb_t *worker_module_ctor;
    module_worker_module_dtor_cb_t *worker_module_dtor;
    module_connection_accept_cb_t *connection_accept;
};
typedef struct module module_t;

extern module_t *modules_registered_list;
extern uint32_t modules_registered_list_size;

static inline __attribute__((always_inline)) module_t* module_get_by_id(
        module_id_t module_id) {
    if (module_id >= modules_registered_list_size) {
        return NULL;
    }

    return &modules_registered_list[module_id];
}

module_t* module_get_by_name(
        const char *name);

module_id_t module_register(
        const char *name,
        module_config_prepare_cb_t *config_prepare,
        module_config_validate_after_load_cb_t *config_validate_after_load,
        module_worker_module_ctor_cb_t *worker_module_ctor,
        module_worker_module_dtor_cb_t *worker_module_dtor,
        module_connection_accept_cb_t *connection_accept);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_H
