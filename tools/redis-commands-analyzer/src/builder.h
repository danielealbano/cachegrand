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
    char    *name;
    command_t **commands;
};

section_t* new_section_p();

bool section_append_command(
        section_t *section,
        int n_command,
        command_t *command);

typedef struct test test_t;
struct test {
    char    *name;
    section_t **sections;
};

test_t* new_test_p();

bool test_append_section(
        test_t *test,
        int n_section,
        section_t *section);

void test_free_sections(
        test_t *test,
        int n_section);

#endif //ANALYZER_BUILDER_H
