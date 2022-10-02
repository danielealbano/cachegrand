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
        const char *filename) {
    char *buffer = NULL;
    size_t string_size, read_size = 0;
    FILE *handler = fopen(filename, "r");

    if (handler) {
        fseek(handler, 0, SEEK_END);
        string_size = ftell(handler);
        rewind(handler);

        buffer = (char*) malloc(sizeof(char) * (string_size + 1));
        do {
            size_t read_size_temp = fread(buffer, sizeof(char), string_size, handler);
            if (read_size_temp <= 0) {
                free(buffer);
                perror("failed to read file");
                exit(EXIT_FAILURE);
            }
            read_size += read_size_temp;
        } while(read_size < string_size);
        buffer[string_size] = '\0';

        fclose(handler);
    }

    return buffer;
}

void support_write_file(
        char* data,
        char* file_path) {
    if (NULL == file_path) {
        file_path = "./cachegrand-benchmark-generator.json";
    }
    FILE *end_file = fopen(file_path, "w");
    if(NULL == end_file) {
        printf("Unable to create file.\n");
        exit(EXIT_FAILURE);
    }

    fputs(data, end_file);
    fclose(end_file);

    printf("File \"%s\" generated.\n", file_path);
}