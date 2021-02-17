#ifndef CACHEGRAND_HASHTABLE_CONFIG_H
#define CACHEGRAND_HASHTABLE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

hashtable_config_t* hashtable_mcmp_config_init();
void hashtable_mcmp_config_free(hashtable_config_t* hashtable_config);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_CONFIG_H
