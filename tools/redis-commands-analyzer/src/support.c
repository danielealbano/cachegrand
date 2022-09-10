//
// Created by Vito Castellano on 10/09/22.
//

#include "support.h"
#include "string.h"

char* replace_char(
        char* str,
        char find,
        char replace) {
    char *current_pos = strchr(str,find);

    while (current_pos) {
        *current_pos = replace;
        current_pos = strchr(current_pos,find);
    }

    return str;
}