#ifndef CACHEGRAND_MODULE_REDIS_CONFIG_H
#define CACHEGRAND_MODULE_REDIS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

bool module_redis_config_prepare(
        config_module_t *module);

bool module_redis_config_validate_after_load(
        config_t *config,
        config_module_t *module);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_CONFIG_H
