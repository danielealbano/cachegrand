/**
 * Copyright (C) 2018-2022 Vito Castellano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <json.h>

#include "matcher.h"
#include "builder.h"
#include "analyzer.h"
#include "support.h"

#include "output.h"

#if DEBUG == 1
void output_stdout_print(
        section_t **sections,
        size_t n_sections) {
    for (int i = 0; i < n_sections; ++i) {
        section_t *section = sections[i];
        printf("SECTION: %s\n", section->name);
        if (section->n_commands > 0) {
            for (int j = 0; j < section->n_commands; ++j) {
                printf("\tCOMMAND: %s\n", section->commands[j]);
            }
        }

        if (section->n_subsections > 0) {
            output_stdout_print(
                    section->subsections,
                    section->n_subsections);
        }
    }

    printf("\n");
}
#endif

void output_json_builder(
        section_t **sections,
        size_t n_sections,
        json_object *j_sections_array) {

    for (int i = 0; i < n_sections; ++i) {
        section_t *section = sections[i];

        struct json_object *j_current_section;
        j_current_section = json_object_new_object();

        json_object_object_add(j_current_section, "section", json_object_new_string(section->name));

        if (section->n_commands > 0) {
            struct json_object *j_commands_array;
            j_commands_array = json_object_new_array();

            for (int j = 0; j < section->n_commands; ++j) {
                json_object_array_add(j_commands_array, json_object_new_string(section->commands[j]));
            }

            json_object_object_add(j_current_section, "commands", j_commands_array);
        }

        if (section->n_subsections > 0) {
            struct json_object *j_subsections_array;
            j_subsections_array = json_object_new_array();

            output_json_builder(
                    section->subsections,
                    section->n_subsections,
                    j_subsections_array);

            json_object_object_add(j_current_section, "subsections", j_subsections_array);
        }

        json_object_array_add(j_sections_array, j_current_section);
    }
}

void output_json(
        tests_t *test_collections,
        char* file_path) {
    struct json_object *obj_final, *tests_array;
    obj_final = json_object_new_object();
    tests_array = json_object_new_array();

    for (int i = 0; i < test_collections->n_tests; ++i) {
        test_t *current_test = test_collections->tests[i];

        struct json_object *j_current_test;
        j_current_test = json_object_new_object();

        struct json_object *j_sections_array;
        j_sections_array = json_object_new_array();

        output_json_builder(
                current_test->sections,
                current_test->n_sections,
                j_sections_array);

        json_object_object_add(j_current_test, current_test->name, j_sections_array);
        json_object_array_add(tests_array, j_current_test);
    }

    json_object_object_add(obj_final, "tests", tests_array);
//    printf("---\n%s\n---\n", json_object_to_json_string(obj_final));

    char *serialized_json;
    serialized_json = strdup(json_object_to_json_string_ext(
                obj_final,
                JSON_C_TO_STRING_PRETTY));

    support_write_file(serialized_json, file_path);
}