//
// Created by Vito Castellano on 11/09/22.
//

#ifndef ANALYZER_BUILDER_H
#define ANALYZER_BUILDER_H

typedef struct command command_t;
struct command {
    char *command;
};

command_t* new_command_p();





typedef struct section section_t;
struct section {
    char *name;

    int n_subsections;
    section_t **subsections;

    int n_commands;
    command_t **commands;
};

section_t* new_section_p();

bool section_append_subsection(
        section_t *section,
        section_t *subsections);

bool section_append_command(
        section_t *section,
        command_t *command);




typedef struct test test_t;
struct test {
    char    *name;
    int n_sections;
    section_t **sections;
};

test_t* new_test_p();

bool test_append_section(
        test_t *test,
        section_t *section);

void test_free_sections(
        test_t *test,
        int n_section);


typedef struct tests tests_t;
struct tests {
    int n_tests;
    test_t **tests;
};

tests_t* new_tests_p();

bool tests_append_test(
        tests_t *tests,
        int n_tests,
        test_t *test);

#endif //ANALYZER_BUILDER_H
