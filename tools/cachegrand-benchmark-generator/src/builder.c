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

section_t* builder_new_section_p() {
    section_t *section;
    section = malloc(sizeof(section_t));
    section->subsections = NULL;
    section->commands = NULL;
    section->n_subsections = 0;
    section->n_commands = 0;
    return section;
}

bool builder_section_append_subsection(
        section_t *section,
        section_t *subsections) {
    if (NULL != subsections) {
        section->subsections = realloc(
                section->subsections,
                (section->n_subsections+1) * sizeof(section_t*));

        section->subsections[section->n_subsections] = subsections;
        section->n_subsections = section->n_subsections+1;
        return true;
    }

    return false;
}

bool builder_section_append_command(
        section_t *section,
        char *command) {
    if (NULL != command) {
        section->commands = realloc(
                section->commands,
                (section->n_commands+1) * sizeof(char*));

        section->commands[section->n_commands] = command;
        section->n_commands = section->n_commands+1;
        return true;
    }

    return false;
}


test_t* builder_new_test_p() {
    test_t *test;
    test = malloc(sizeof(test_t));
    test->sections = NULL;
    test->n_sections = 0;
    return test;
}

bool builder_test_append_section(
        test_t *test,
        section_t *section) {
    if (NULL != section) {
        test->sections = realloc(
                test->sections,
                (test->n_sections+1) * sizeof(section_t*));

        test->sections[test->n_sections] = section;
        test->n_sections = test->n_sections+1;
        return true;
    }

    return false;
}

tests_t* builder_new_tests_p() {
    tests_t *tests;
    tests = malloc(sizeof(tests_t));
    tests->tests = NULL;
    tests->n_tests = 0;
    return tests;
}

bool builder_tests_append_test(
        tests_t *tests,
        test_t *test) {
    if (NULL != test) {
        tests->tests = realloc(
                tests->tests,
                (tests->n_tests+1) * sizeof(test_t*));

        tests->tests[tests->n_tests] = test;
        tests->n_tests = tests->n_tests+1;
        return true;
    }

    return false;
}