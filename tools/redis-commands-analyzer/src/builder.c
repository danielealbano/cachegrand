/**
 * Copyright (C) 2018-2022 Vito Castellano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdbool.h>

#include "builder.h"

section_t* new_section_p() {
    section_t *section;
    section = malloc(sizeof(section_t));
    section->subsections = malloc(sizeof(section_t));
    section->commands = malloc(sizeof(char*));
    section->n_subsections = 0;
    section->n_commands = 0;
    return section;
}

bool section_append_subsection(
        section_t *section,
        section_t *subsections) {
    section->subsections = realloc(
            section->subsections,
            (section->n_subsections+1) * sizeof(section_t));

    section->subsections[section->n_subsections] = subsections;
    if (section->subsections[section->n_subsections] != NULL) {
        section->n_subsections = section->n_subsections+1;
        return true;
    }

    return false;
}

bool section_append_command(
        section_t *section,
        char *command) {
    section->commands = realloc(
            section->commands,
            (section->n_commands+1) * sizeof(char*));

    section->commands[section->n_commands] = command;
    if (section->commands[section->n_commands] != NULL) {
        section->n_commands = section->n_commands+1;
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
    test->sections = realloc(
            test->sections,
            (test->n_sections+1) * sizeof(section_t));

    test->sections[test->n_sections] = section;
    if (test->sections[test->n_sections] != NULL) {
        test->n_sections = test->n_sections+1;
        return true;
    }

    return false;
}

tests_t* new_tests_p() {
    tests_t *tests;
    tests = malloc(sizeof(tests_t));
    tests->tests = malloc(1 * sizeof(test_t));
    tests->n_tests = 0;
    return tests;
}

bool tests_append_test(
        tests_t *tests,
        test_t *test) {
    tests->tests = realloc(
            tests->tests,
            (tests->n_tests+1) * sizeof(test_t));

    tests->tests[tests->n_tests] = test;
    if (tests->tests[tests->n_tests] != NULL) {
        tests->n_tests = tests->n_tests+1;
        return true;
    }

    return false;
}