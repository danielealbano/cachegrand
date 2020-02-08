// Author: Nick Strupat
// Date: October 29, 2010
// Returns the cache line size (in bytes) of the processor, or 0 on failure

#ifndef CACHEGRAND_CACHELINESIZE_H
#define CACHEGRAND_CACHELINESIZE_H

#ifdef __cplusplus
extern "C" {
#endif

size_t cacheline_size();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_CACHELINESIZE_H
