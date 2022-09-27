#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "matcher.h"
#include "builder.h"
#include "support.h"

#include "analyzer.h"

char    *tests_lists[] = {
        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-del.cpp",
        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-decrby.cpp",
        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-del.cpp",
        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-append.cpp",
};


test_t* read_content(char *file_path) {
    char *string = read_file(file_path);

    test_t *current_test;
    current_test = new_test_p();

    // Testing recursive
    int starting_padding = 4;
    recursive_match(string, starting_padding, current_test, NULL);

    return current_test;
}

void recursive_print(section_t **sections, size_t n_sections) {
        for (int i = 0; i < n_sections; ++i) {
            section_t *section;
            section = sections[i];

            printf("SECTION: %s\n", section->name);
            if (section->n_commands > 0) {
                for (int j = 0; j < section->n_commands; ++j) {
                    printf("\tCOMMAND: %s\n", section->commands[j]);
                }
            }

            if (section->n_subsections > 0) {
                recursive_print(section->subsections, section->n_subsections);
            }
        }

        // Check Subsection
        printf("\n");
}

int recursive_match(const char *body, int padding, test_t *current_test, section_t *father_section) {
    // Match SECTIONS
    matcher_t *match_results = get_sections(body, padding);
    if (match_results->n_matches <= 0) {
        free(match_results);
        return 0;
    }

    for (int i = 0; i < match_results->n_matches; ++i) {
        section_t *new_current_section_p;
        new_current_section_p = new_section_p();

        // Match SECTION name
        char *section_name;
        section_name = get_section_name(match_results->matches[i]);
        if (NULL == section_name) continue;

        new_current_section_p->name = section_name;

        // Match REQUIRE section
        matcher_t *section_requires = get_requires_section(match_results->matches[i], padding);
        if (section_requires->n_matches > 0) {
            for (int j = 0; j < section_requires->n_matches; ++j) {
                // Match COMMAND
                char *command = get_require_command(section_requires->matches[j]);
                if (NULL == command) continue;

                section_append_command(new_current_section_p, command);
            }
        }
        free(section_requires);

        if (NULL != father_section) {
            section_append_subsection(father_section, new_current_section_p);
        } else {
            test_append_section(current_test, new_current_section_p);
        }

        recursive_match(
                match_results->matches[i],
                padding*2,
                current_test,
                new_current_section_p);
    }

    free(match_results);
}

int main() {
    printf("Start main \n");

    tests_t *new_tests;
    new_tests = new_tests_p();

    size_t n_tests = sizeof(tests_lists) / sizeof(char*);
    for (int i = 0; i < n_tests; ++i) {
        test_t* test;
        test = read_content(tests_lists[i]);

        tests_append_test(new_tests, test);
    }

    // Print results
    for (int i = 0; i < new_tests->n_tests; ++i) {
        test_t *current_test;
        current_test = new_tests->tests[i];
        recursive_print(current_test->sections, current_test->n_sections);
        puts("##############################################");
    }

    return EXIT_SUCCESS;
}

