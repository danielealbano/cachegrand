#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "matcher.h"
#include "builder.h"
#include "support.h"

#include "analyzer.h"

FILE    *current_file;
tests_t *tests;
char    *tests_lists[] ={
//        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-del.cpp",
        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-append.cpp"
};


void read_content() {
    char *string = read_file("../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-append.cpp");
//    char *string = read_file("../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-decrby.cpp");
//    char *string = read_file("../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-copy.cpp");

    // Testing recursive
    int starting_padding = 4;
    recursive_match(string, starting_padding);
}

int recursive_match(const char *body, int padding) {
    // Match SECTIONS
    matcher_t *match_results = get_sections(body, padding);
    if (match_results->n_matches <= 0) {
        printf("NO SUB-SECTIONS FOUND\n");
        free(match_results);
        return 0;
    }

    printf("%d Sections found!\n", match_results->n_matches);
    for (int i = 0; i < match_results->n_matches; ++i) {
        puts("---------------------------------------------------");
        // Match SECTION name
        char *section_name;
        section_name = get_section_name(match_results->matches[i]);
        if (NULL == section_name) {
            printf("[!] section name not found.. skip");
            continue;
        }
        printf("SECTION: %s\n", section_name);

        // Match REQUIRE section
        matcher_t *section_requires = get_requires_section(match_results->matches[i], padding);
        if (section_requires->n_matches > 0) {
            printf("\t%d REQUIRE found: \n", section_requires->n_matches);
            for (int j = 0; j < section_requires->n_matches; ++j) {
                // Match COMMAND
                char *command = get_require_command(section_requires->matches[j]);
                if (NULL == command) {
                    printf("[!] command not found.. skip");
                    continue;
                }
                printf("\t\tCOMMAND: %s\n", command);
            }
        } else {
            printf("No require found for this section\n");
        }
        free(section_requires);

        recursive_match(match_results->matches[i], padding*2);
    }

    puts("End");
    free(match_results);
}

int main() {
    printf("Start main \n");

    // Init tests
    tests = new_tests_p();

    size_t n_tests = sizeof(tests_lists) / sizeof(char*);
    for (int i = 0; i < n_tests; ++i) {
        // Open file to elaborate
        current_file = fopen(tests_lists[i], "rb");
        if (NULL == current_file) {
            printf("file can't be opened \n");
            continue;
        }

        read_content();

        fclose(current_file);
    }

//    for (int i = 0; i < tests->n_tests; ++i) {
//        printf("Test Founded: %s \n", tests->tests[i]->name);
//        for (int j = 0; j < tests->tests[i]->n_sections; ++j) {
//            printf("Section Fouded: %s \n", tests->tests[i]->sections[j]->name);
//            for (int k = 0; k < tests->tests[i]->sections[j]->n_commands; ++k) {
//                printf("Command Fouded: %s \n", tests->tests[i]->sections[j]->commands[k]->command);
//            }
//        }
//    }

    return EXIT_SUCCESS;
}

