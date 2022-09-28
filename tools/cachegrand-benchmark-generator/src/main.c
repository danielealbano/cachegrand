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

char *test_lists[] = {
        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-del.cpp",
        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-decrby.cpp",
        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-del.cpp",
        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-append.cpp",
};

int main() {
    tests_t *test_collections = builder_new_tests_p();

    size_t n_tests = sizeof(test_lists) / sizeof(char*);
    for (int i = 0; i < n_tests; ++i) {
        test_t* test = analyzer_analyze(test_lists[i]);
        builder_tests_append_test(test_collections, test);
//        output_stdout_print(test->sections, test->n_sections);
    }

    output_json_print(test_collections);

    return EXIT_SUCCESS;
}