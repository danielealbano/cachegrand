#ifndef CACHEGRAND_HASHTABLE_GC_H
#define CACHEGRAND_HASHTABLE_GC_H

#ifdef __cplusplus
extern "C" {
#endif

void hashtable_garbage_collect_neighborhood(hashtable_t* hashtable, hashtable_bucket_index_t bucket_index);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_GC_H
