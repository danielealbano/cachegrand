#include <stdlib.h>
#include <stdbool.h>

#include "misc.h"

#include "module_redis_utils.h"

// Derived from https://www.codeproject.com/Articles/5163931/Fast-String-Matching-with-Wildcards-Globs-and-Giti
// TODO: should handle UTF-8? Redis code seems doesn't do it;
bool module_redis_glob_match(
        char *string,
        size_t string_length,
        char *pattern,
        size_t pattern_length) {
    char *string_backup = NULL, *pattern_backup = NULL;
    size_t string_length_backup = 0, pattern_length_backup = 0;

    if (unlikely(pattern_length == 0 || string_length == 0)) {
        return false;
    }

    while (string_length > 0) {
        switch (*pattern) {
            case '*':
                string_backup = string;
                pattern_backup = ++pattern;
                string_length_backup = string_length;
                pattern_length_backup = --pattern_length;
                continue;

            case '?':
                string++;
                pattern++;
                string_length--;
                pattern_length--;
                continue;

            case '[':
                if (pattern_length <= 1) {
                    break;
                }

                bool matched = false, reverse = false;

                pattern++;
                pattern_length--;

                if (*pattern == '^') {
                    reverse = true;
                    pattern++;
                    pattern_length--;
                }

                matched = false;
                int prev_char = -1;
                while(pattern_length > 0 && *pattern != ']') {
                    switch(*pattern) {
                        case '-':
                            if (unlikely(prev_char == -1)) {
                                continue;
                            }

                            pattern++;
                            pattern_length--;
                            if (unlikely(pattern_length == 0)) {
                                continue;
                            }

                            if (!matched) {
                                if (*string >= prev_char && *string <= *pattern) {
                                    matched = true;
                                }
                            }
                            break;

                        case '\\':
                            pattern++;
                            pattern_length--;
                            if (unlikely(pattern_length == 0)) {
                                continue;
                            }

                        default:
                            if (!matched) {
                                if (*string == *pattern) {
                                    matched = true;
                                }
                            }

                            prev_char = (int)*pattern;
                            pattern++;
                            pattern_length--;
                            break;
                    }
                }

                if (*pattern != ']') {
                    break;
                }

                if (reverse) {
                    matched = matched ? false : true;
                }

                if (!matched) {
                    break;
                }

                pattern++;
                pattern_length--;
                string++;
                string_length--;
                continue;

            case '\\':
                pattern++;
                pattern_length--;

                if (pattern_length == 0) {
                    break;
                }

            default:
                if (*pattern != *string) {
                    break;
                }

                string++;
                pattern++;
                string_length--;
                pattern_length--;
                continue;
        }

        if (pattern_backup == NULL) {
            return false;
        }

        string = ++string_backup;
        pattern = pattern_backup;
        string_length = --string_length_backup;
        pattern_length = pattern_length_backup;
    }

    while (pattern_length > 0 && *pattern == '*') {
        pattern++;
        pattern_length--;
    }

    return pattern_length == 0 ? true : false;
}
