#ifndef CACHEGRAND_HASHTABLE_CONFIG_H
#define CACHEGRAND_HASHTABLE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif


#define HASHTABLE_CONFIG_CACHELINES_PRIMENUMBERS_MAP_FOREACH(list, index, list_value, ...) { \
    hashtable_config_cachelines_to_probe_t list[] = { HASHTABLE_CONFIG_CACHELINES_PRIMENUMBERS_MAP }; \
    for(uint64_t index = 0; index < HASHTABLE_CONFIG_CACHELINES_PRIMENUMBERS_MAP_SIZE; index++) { \
        hashtable_config_cachelines_to_probe_t list_value = list[index]; \
__VA_ARGS__ \
    } \
}

void hashtable_config_prefill_cachelines_to_probe_with_defaults(hashtable_config_t* hashtable_config);
hashtable_config_t* hashtable_config_init();
void hashtable_config_free(hashtable_config_t* hashtable_config);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_CONFIG_H
