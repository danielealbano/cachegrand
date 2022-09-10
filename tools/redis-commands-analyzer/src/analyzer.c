#include "stdio.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "matcher.h"
#include "builder.h"

#include "analyzer.h"

char *tests_lists[] ={
        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-del.cpp",
//        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-append.cpp"
};

FILE *current_file;
tests_t *tests;
int n_tests;

bool open_file(char *file_path) {
    current_file = fopen(file_path, "r");
    if (NULL == current_file) {
        printf("file can't be opened \n");
        return false;
    }

    return true;
}

void close_file() {
    fclose(current_file);
}

void read_file_content() {
    char line[500];
    int n_sections = 0;
    int n_commands = 0;
    section_t *current_section = NULL;
    test_t *current_test = NULL;

    while (fgets(line, 500, current_file) != NULL) {
        // Check if is a new test case
        if (is_testcase(line)) {
            current_test = new_test_p();
            if (current_test->sections == NULL) {
                printf("error test sections memory allocator");
                exit(EXIT_FAILURE);
            }

            current_test->name = get_testcase_name(line);
        }

        // Check if is a new section
        if (is_section(line)) {
            // Reset number commands
            n_commands = 0;

            // Get Section Name
            char *section_name;
            section_name = get_section_name(line);

            // Check if section is differnte than previous
            if (current_section) {
                if (section_name != current_section->name) {
                    if (!test_append_section(
                            current_test,
                            n_sections,
                            current_section)) {
                        printf("memory error appending sections");
                        exit(EXIT_FAILURE);
                    }

                    // TODO: if not present any test in this section probabily is a child
                    ++n_sections;
                }
            }

            // Save section information
            current_section = new_section_p();
            current_section->name = section_name;
        }

        // If pass this probabily we have a section
        if (is_command(line) && current_section->name) {
            char *command_data;
            command_data = get_command(line);

            command_t *command;
            command = new_command_p();
            command->command = command_data;

            if (!section_append_command(
                    current_section,
                    n_commands,
                    command)) {
                printf("memory error appending sections");
                exit(EXIT_FAILURE);
            }

            ++n_commands;
        }
    }

    // Append last data and free pointer
    if (!test_append_section(
            current_test,
            n_sections,
            current_section)) {
        printf("memory error appending sections");
        exit(EXIT_FAILURE);
    }

    // Append single test to tests
    if (!tests_append_test(
            tests,
           n_tests,
            current_test)) {
        printf("memory error appending test to tests");
        exit(EXIT_FAILURE);
    }

//    test_free_sections(current_test, n_sections);
//    free(tests);
}

int main() {
    printf("Start main \n");

    // Init tests
    tests = new_tests_p();

    size_t n = sizeof(tests_lists) / sizeof(char*);
    for (int i = 0; i < n; ++i) {
        if (open_file(tests_lists[i])) {
            read_file_content();
            close_file();

            ++n_tests;
        }
    }

    for (int i = 0; i < tests->n_tests; ++i) {
        printf("Test Founded: %s \n", tests->tests[i]->name);
        for (int j = 0; j < tests->tests[i]->n_sections; ++j) {
            printf("Section Fouded: %s \n", tests->tests[i]->sections[j]->name);
            for (int k = 0; k < tests->tests[i]->sections[j]->n_commands; ++k) {
                printf("Command Fouded: %s \n", tests->tests[i]->sections[j]->commands[k]->command);
            }
        }
    }

    return EXIT_SUCCESS;
}

