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

#include "matcher.h"
#include "builder.h"
#include "support.h"

#include "analyzer.h"

test_t* analyzer_analyze(
        char *file_path) {
    char *body = support_read_file(file_path);
    test_t *test = builder_new_test_p();

    analyzer_recursive_match(
            body,
            START_PADDING,
            test,
            NULL);

    return test;
}

void analyzer_recursive_print(
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
                analyzer_recursive_print(
                        section->subsections,
                        section->n_subsections);
            }
        }

        // Check Subsection
        printf("\n");
}

int analyzer_recursive_match(
        const char *body,
        int padding,
        test_t *current_test,
        section_t *father_section) {
    // Match SECTIONS
    matcher_t *match_results = matcher_get_sections(body, padding);
    if (match_results->n_matches <= 0) {
        free(match_results);
        return 0;
    }

    for (int i = 0; i < match_results->n_matches; ++i) {
        section_t *new_current_section_p = builder_new_section_p();
        // Match SECTION name
        char *section_name = matcher_get_section_name(match_results->matches[i]);
        if (NULL == section_name) continue;
        new_current_section_p->name = section_name;

        // Match REQUIRE section
        matcher_t *section_requires = matcher_get_requires_section(match_results->matches[i], padding);
        if (section_requires->n_matches > 0) {
            for (int j = 0; j < section_requires->n_matches; ++j) {
                // Match COMMAND
                char *command = matcher_get_require_command(section_requires->matches[j]);
                if (NULL == command) continue;
                builder_section_append_command(
                        new_current_section_p, command);
            }
        }
        free(section_requires);

        if (NULL != father_section) {
            builder_section_append_subsection(
                    father_section,
                    new_current_section_p);
        } else {
            builder_test_append_section(
                    current_test,
                    new_current_section_p);
        }

        analyzer_recursive_match(
                match_results->matches[i],
                padding*2,
                current_test,
                new_current_section_p);
    }

    free(match_results);
}