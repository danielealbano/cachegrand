//
// Created by Vito Castellano on 11/09/22.
//

#include <stdlib.h>
#include <stdbool.h>

#include "builder.h"

command_t* new_command_p() {
    command_t *command;
    command = malloc(sizeof(command_t));
    return command;
}

section_t* new_section_p() {
    section_t *section;
    section = malloc(sizeof(section_t));
    return section;
}

bool section_append_command(
        section_t *section,
        int n_command,
        command_t *command) {
    section->commands = realloc(section->commands, n_command+1 * sizeof(section_t));
    section->commands[n_command] = command;
    if (section->commands[n_command] != NULL) {
        return true;
    }

    return false;
}


test_t* new_test_p() {
    test_t *test;
    test = malloc(sizeof(test_t));
    test->sections = (section_t**) malloc(1 * sizeof(section_t));
    return test;
}

bool test_append_section(
        test_t *test,
        int n_section,
        section_t *section) {
    test->sections = realloc(test->sections, n_section+1 * sizeof(section_t));
    test->sections[n_section] = section;
    if (test->sections[n_section] != NULL) {
        return true;
    }

    return false;
}

void test_free_sections(
        test_t *test,
        int n_section) {
    for (int i = 0; i < n_section; ++i) {
        free(test->sections[i]);
    }
}