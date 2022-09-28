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

#include "matcher.h"
#include "builder.h"
#include "analyzer.h"
#include "support.h"
#include "output.h"

int main(int argc, char **argv) {
    printf("### Cachegrand Benchmark Generator ###\n");
    size_t n_tests = 0;
    char **test_lists = malloc(sizeof(char*));

    for (int i = 1; i < argc; ++i) {
        test_lists = realloc(test_lists, i*sizeof(char*));
        test_lists[i-1] = argv[i];
        ++n_tests;
    }

    if (n_tests > 0) {
        printf("[%zu] Total tests!\n", n_tests);
        tests_t *test_collections = builder_new_tests_p();
        for (int i = 0; i < n_tests; ++i) {
            printf("[%i/%zu]", i+1, n_tests);
            test_t* test = analyzer_analyze(test_lists[i]);
            printf(" \"%s\" ", test->name);
            builder_tests_append_test(test_collections, test);
            printf("...OK\n");
            //output_stdout_print(test->sections, test->n_sections);
        }

        output_json_print(test_collections);
        printf("Generation done. Bye.");
        return EXIT_SUCCESS;
    }

    printf("No tests loaded. Bye.");
    return EXIT_FAILURE;
}