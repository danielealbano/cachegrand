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


void open_file() {

}

void read_content() {
    FILE *file;
    char line[500];

//    file = fopen("../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-append.cpp", "r");
    file = fopen("../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-del.cpp", "r");
    if (NULL == file) {
        printf("file can't be opened \n");
    }

    test_t **tests;
    tests = (test_t**) malloc(1 * sizeof(test_t));


    int n_sections = 0;
    int n_commands = 0;
    section_t *current_section = new_section_p();
    test_t *current_test;
    while (fgets(line, 500, file) != NULL) {
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

            printf("---- NEW SECTION ---- \n");
            // Get Section Name
            char *section_name;
            section_name = get_section_name(line);

            // Check if section is differnte than previous
            if (section_name != current_section->name && current_section->name) {
                if (!test_append_section(
                        current_test,
                        n_sections,
                        current_section)) {
                    printf("memory error appending sections");
                    exit(EXIT_FAILURE);
                }

                // TODO: if not present any test in this section probabily is a child

                // Init new section
                current_section = new_section_p();
                ++n_sections;
            }

            // Save section information
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

    // Append last section and free pointer
    if (!test_append_section(
            current_test,
            n_sections,
            current_section)) {
        printf("memory error appending sections");
        exit(EXIT_FAILURE);
    }

    fclose(file);
    tests[0] = current_test;

    for (int i = 0; i <= n_sections; ++i) {
        printf("QUIIIi %s", tests[0]->sections[i]->name);
    }

    test_free_sections(current_test, n_sections);
    free(tests);
}

int main() {
    printf("Start main \n");

    read_content();

//    printf("SECTIONS FOUD: %u \n", foundedSection);
//    for (int i = 0; i < foundedSection; ++i) {
//        printf("%s", sections[i]);
//
//        free(sections[i]);
//    }

    return EXIT_SUCCESS;
}

