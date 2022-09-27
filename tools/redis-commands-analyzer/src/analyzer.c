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

#include "matcher.h"
#include "builder.h"
#include "support.h"

#include "analyzer.h"

char *tests_lists[] = {
    "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-del.cpp",
    "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-decrby.cpp",
    "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-del.cpp",
    "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-append.cpp",
};

test_t* anlyzer_analyze(
        char *file_path) {
    char *body = support_read_file(file_path);
    test_t *test = builder_new_test_p();

    anlyzer_recursive_match(
            body,
            START_PADDING,
            test,
            NULL);

    return test;
}

void anlyzer_recursive_print(
        section_t **sections,
        size_t n_sections) {
        for (int i = 0; i < n_sections; ++i) {
            section_t *section = sections[i];
            printf("SECTION: %s\n", section->name);
            if (section->n_commands > 0) {
                for (int j = 0; j < section->n_commands; ++j) {
                    printf("\tCOMMAND: %s\n", section->commands[j]);
                }
            }

            if (section->n_subsections > 0) {
                anlyzer_recursive_print(
                        section->subsections,
                        section->n_subsections);
            }
        }

        // Check Subsection
        printf("\n");
}

int anlyzer_recursive_match(
        const char *body,
        int padding,
        test_t *current_test,
        section_t *father_section) {
    // Match SECTIONS
    matcher_t *match_results = matcher_get_sections(body, padding);
    if (match_results->n_matches <= 0) {
        free(match_results);
        return 0;
    }

    for (int i = 0; i < match_results->n_matches; ++i) {
        section_t *new_current_section_p = builder_new_section_p();
        // Match SECTION name
        char *section_name = matcher_get_section_name(match_results->matches[i]);
        if (NULL == section_name) continue;
        new_current_section_p->name = section_name;

        // Match REQUIRE section
        matcher_t *section_requires = matcher_get_requires_section(match_results->matches[i], padding);
        if (section_requires->n_matches > 0) {
            for (int j = 0; j < section_requires->n_matches; ++j) {
                // Match COMMAND
                char *command = matcher_get_require_command(section_requires->matches[j]);
                if (NULL == command) continue;
                builder_section_append_command(
                        new_current_section_p, command);
            }
        }
        free(section_requires);

        if (NULL != father_section) {
            builder_section_append_subsection(
                    father_section,
                    new_current_section_p);
        } else {
            builder_test_append_section(
                    current_test,
                    new_current_section_p);
        }

        anlyzer_recursive_match(
                match_results->matches[i],
                padding*2,
                current_test,
                new_current_section_p);
    }

    free(match_results);
}

int main() {
    tests_t *test_collections = builder_new_tests_p();

    size_t n_tests = sizeof(tests_lists) / sizeof(char*);
    for (int i = 0; i < n_tests; ++i) {
        test_t* test = anlyzer_analyze(
                tests_lists[i]);

        builder_tests_append_test(test_collections, test);
    }

    for (int i = 0; i < test_collections->n_tests; ++i) {
        test_t *current_test = test_collections->tests[i];
        anlyzer_recursive_print(
                current_test->sections,
                current_test->n_sections);
        puts("##############################################");
    }

    return EXIT_SUCCESS;
}

