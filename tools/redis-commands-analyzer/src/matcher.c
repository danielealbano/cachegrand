//
// Created by Vito Castellano on 11/09/22.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <regex.h>

#include "support.h"

#include "matcher.h"

char** pattern_matcher(
        char *pattern,
        int nmatch,
        char *line) {
    char **result;
    char *str;
    regex_t regex;
    regmatch_t *match;

    if (0 != (regcomp(&regex, pattern, REG_EXTENDED))) {
        printf("regcomp() failed, returning nonzero (%s)\n", pattern);
        exit(EXIT_FAILURE);
    }

    str = strdup(line);
    match = malloc(nmatch * sizeof(*match));
    if (0 != (regexec(&regex,str, nmatch,match,0))) {
        return NULL;
    }

    result = malloc(sizeof(*result));
    if (!result) {
        fprintf(stderr, "Out of memory !");
        return NULL;
    }

    for (int i = 0; i < nmatch; ++i) {
        ((char**) result)[i] = "";
        if (match[i].rm_so >= 0) {
            str[match[i].rm_eo] = 0;
            ((char**)result)[i] = str + match[i].rm_so;
            // printf("%s\n", string + match[i].rm_so);
        }
    }

    return result;
}

bool is_testcase(
        char *line) {
    char **res;
    char *pattern = "TEST_CASE_METHOD";
    int  nmatch   = 1;

    res = pattern_matcher(pattern, nmatch, line);

    return res ? true : false;
}

char* get_testcase_name(
        char *line) {
    char **res;
    char *pattern = "\"([^\"]*)\"";
    int  nmatch   = 2;

    res = pattern_matcher(pattern, nmatch, line);
    if (res) {
        return res[nmatch-1];
    }

    return NULL;
}

char* get_section_name(
        char *line) {
    char **res;
    char *pattern = "\"([^\"]*)\"";
    int  nmatch   = 2;

    res = pattern_matcher(pattern, nmatch, line);
    if (res) {
        return res[nmatch-1];
    }

    return NULL;
}

bool is_section(
        char *line) {
    char **res;
    char *pattern = "SECTION";
    int  nmatch   = 1;

    res = pattern_matcher(pattern, nmatch, line);

    return res ? true : false;
}

bool is_command(
        char *line) {
    char **res;
    char *pattern = "std::vector<std::string>";
    int  nmatch   = 1;

    res = pattern_matcher(pattern, nmatch, line);

    return res ? true : false;
}

char* get_command(
        char *line) {
    char **res;
    char *pattern = "\"(.*?)\"";
    int  nmatch   = 1;

    res = pattern_matcher(pattern, nmatch, line);

    char *command = NULL;
    command = malloc(500);
    if (res) {
        for (int i = 0; i < nmatch; ++i) {
            strcat(command,res[i]);
        }
    }

    //TODO: make it better
    command = replace_char(command, '"', ' ');
    command = replace_char(command, ',', ' ');

    return command;
}