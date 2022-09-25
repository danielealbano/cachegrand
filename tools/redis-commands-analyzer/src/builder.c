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
    section->n_subsections = 0;
    section->n_commands = 0;
    section->subsections = malloc(sizeof(section_t));
    return section;
}

bool section_append_subsection(
        section_t *section,
        section_t *subsections) {
    section->subsections = realloc(section->subsections, section->n_subsections+1 * sizeof(section_t));
    section->subsections[section->n_subsections] = subsections;
    if (section->subsections[section->n_subsections] != NULL) {
        section->n_subsections = section->n_subsections+1;
        return true;
    }

    return false;
}

bool section_append_command(
        section_t *section,
        command_t *command) {
    section->n_commands = section->n_commands+1;
    section->commands = realloc(section->commands, section->n_commands+1 * sizeof(section_t));
    section->commands[section->n_commands] = command;
    if (section->commands[section->n_commands] != NULL) {
        return true;
    }

    return false;
}


test_t* new_test_p() {
    test_t *test;
    test = malloc(sizeof(test_t));
    test->sections = (section_t**) malloc(1 * sizeof(section_t));
    test->n_sections = 0;
    return test;
}

bool test_append_section(
        test_t *test,
        section_t *section) {
    test->sections = realloc(test->sections, test->n_sections+1 * sizeof(section_t));
    test->sections[test->n_sections] = section;
    if (test->sections[test->n_sections] != NULL) {
        test->n_sections = test->n_sections+1;
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

tests_t* new_tests_p() {
    tests_t *tests;
    tests = (tests_t*) malloc(1 * sizeof(tests_t));
    tests->tests = (test_t**) malloc(1 * sizeof(test_t));
    return tests;
}

bool tests_append_test(
        tests_t *tests,
        int n_tests,
        test_t *test) {
    tests->n_tests = n_tests+1;
    tests->tests[n_tests] = realloc(tests->tests[n_tests], n_tests+1 * sizeof(test_t));
    tests->tests[n_tests] = test;
    if (tests->tests[n_tests] != NULL) {
        return true;
    }

    return false;
}