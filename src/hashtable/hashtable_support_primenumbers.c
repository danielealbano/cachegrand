#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <t1ha.h>

#include "hashtable.h"
#include "hashtable_support_primenumbers.h"

bool hashtable_support_primenumbers_valid(uint64_t number) {
    return number <= HASHTABLE_PRIMENUMBERS_MAX ? true : false;
}

uint64_t hashtable_support_primenumbers_next(uint64_t number) {
    HASHTABLE_PRIMENUMBERS_FOREACH(primenumbers, index, {
        if (number < primenumbers[index]) {
            return primenumbers[index];
        }
    });

    return 0;
}

uint64_t hashtable_support_primenumbers_mod(uint64_t number, uint64_t prime) {
    HASHTABLE_PRIMENUMBERS_FOREACH(primenumbers, index, {
        if (prime == primenumbers[index]) {
            return number % primenumbers[index];
        }
    });

    return 0;
}
