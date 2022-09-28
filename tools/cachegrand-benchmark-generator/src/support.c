/**
 * Copyright (C) 2018-2022 Vito Castellano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "support.h"

char* support_read_file(
        char *filename) {
    char *buffer = NULL;
    int string_size, read_size;
    FILE *handler = fopen(filename, "r");

    if (handler) {
        fseek(handler, 0, SEEK_END);
        string_size = ftell(handler);
        rewind(handler);

        buffer = (char*) malloc(sizeof(char) * (string_size + 1) );
        read_size = fread(buffer, sizeof(char), string_size, handler);
        buffer[string_size] = '\0';

        if (string_size != read_size) {
            free(buffer);
            buffer = NULL;
        }

        fclose(handler);
    }

    return buffer;
}

void support_write_file(
        char* data) {
    char *out_filename = "./cachegrand-benchmark-generator.json";
    FILE *end_file = fopen(out_filename, "w");
    if(end_file == NULL) {
        printf("Unable to create file.\n");
        exit(EXIT_FAILURE);
    }

    fputs(data, end_file);
    fclose(end_file);

    printf("File \"%s\" generated.\n", out_filename);
}