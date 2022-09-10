//
// Created by Vito Castellano on 11/09/22.
//

#ifndef ANALYZER_MATCHER_H
#define ANALYZER_MATCHER_H

char** pattern_matcher(
        char *pattern,
        int nmatch,
        char *line);

bool is_testcase(
        char *line);

char* get_testcase_name(
        char *line);

char* get_section_name(
        char *line);

bool is_section(
        char *line);

bool is_command(
        char *line);

char* get_command(
        char *line);

#endif //ANALYZER_MATCHER_H
