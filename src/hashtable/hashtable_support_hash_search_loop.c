#include <stdint.h>

__attribute__((noinline)) int8_t hashtable_support_hash_search_loop(uint32_t hash, uint32_t* hashes) {
    for(uint8_t index = 0; index < 14; index++) {
        if (hashes[index] == hash) {
            return index;
        }
    }

    return -1;
}
