/**
 * Copyright (C) 2018-2022 Vito Castellano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <json.h>
#include <unistd.h>

#include "all_tests.h"
#include "matcher.h"
#include "builder.h"
#include "analyzer.h"
#include "support.h"
#include "output.h"

int main(int argc, char **argv) {
    printf("### Cachegrand Benchmark Generator ###\n");

    int opt;
    char* output_file_path = NULL;
    while ((opt = getopt(argc, argv, "o:")) != -1) {
        switch (opt) {
            case 'o': output_file_path = optarg; break;
            default:
                fprintf(stderr, "Usage: %s [-o] [./output-file-path.json]\n\n", argv[0]);
        }
    }

    size_t n_tests = sizeof(all_tests) / sizeof(char*);
    if ((int) n_tests > 0) {
        printf("[%zu] Total tests!\n", n_tests);
        tests_t *test_collections = builder_new_tests_p();
        for (int i = 0; i < n_tests; ++i) {
            printf("[%i/%zu]", i+1, n_tests);
            test_t* test = analyzer_analyze(all_tests[i]);
            if (NULL == test) {
                printf(" Not a valid test ...SKIP -> %s\n", all_tests[i]);
                continue;
            }
            printf(" \"%s\" ", test->name);
            builder_tests_append_test(test_collections, test);
            printf("...OK\n");
        }

        output_json(test_collections, output_file_path);
        printf("Generation done. Bye.\n");
        return EXIT_SUCCESS;
    }

    printf("No tests loaded. Bye.\n");
    return EXIT_FAILURE;
}