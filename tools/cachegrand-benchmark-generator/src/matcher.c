/**
 * Copyright (C) 2018-2022 Vito Castellano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "support.h"
#include "matcher.h"
#include <pcre2.h>

const char* MATCH_SECTION_PATTERN = "^\\s{%d}SECTION\\(\".*\"\\)\\s{\\n(?:[\\s\\S]*?^\\s{%d}\\}\\n)";
const char* MATCH_SECTION_NAME_PATTERN = "(?<=SECTION\\(\\\")[^\"]*";
const char* MATCH_SECTION_REQUIRE_PATTERN = "^\\s{%d}REQUIRE\\([\\s\\S]*?\\);";
const char* MATCH_SECTION_COMMAND_PATTERN = "(?<=std::vector<std::string>{)[^}]*";
const char* MATCH_COMMAND_PATTERN = "[^\"\\\\,]+";
const char* MATCH_TEST_NAME = "(?<=TEST_CASE_METHOD\\(TestModulesRedisCommandFixture,\\s\\\")[^\"]*";

matcher_t* matcher_match(
        const char *content,
        const char *regex) {
    pcre2_code *re;
    PCRE2_SPTR pattern;
    PCRE2_SPTR subject;

    int crlf_is_newline;
    int errornumber;
    int i;
    int rc;
    int utf8;

    uint32_t option_bits;
    uint32_t newline;

    PCRE2_SIZE erroroffset;
    PCRE2_SIZE *ovector = NULL;

    size_t subject_length;
    pcre2_match_data *match_data;

    pattern = (PCRE2_SPTR)regex;
    subject = (PCRE2_SPTR)content;
    subject_length = strlen((char *)subject);

    matcher_t *final_matches;
    final_matches = malloc(sizeof(matcher_t));
    final_matches->n_matches = 0;
    final_matches->matches = NULL;

    /***********************
    * COMPILE PATTERN
    ************************/
    re = pcre2_compile(
            pattern,               /* the pattern */
            PCRE2_ZERO_TERMINATED, /* indicates pattern is zero-terminated */
            PCRE2_MULTILINE,       /* default options */
            &errornumber,          /* for error number */
            &erroroffset,          /* for error offset */
            NULL);                 /* use default compile context */

    if (re == NULL) {
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
        printf("PCRE2 compilation failed at offset %d: %s\n", (int) erroroffset, buffer);
        return NULL;
    }

    /***********************
    * MATCH DATA
    ************************/
    match_data = pcre2_match_data_create_from_pattern(re, NULL);

    /* Before running the loop, check for UTF-8 and whether CRLF is a valid newline
    sequence. First, find the options with which the regex was compiled and extract
    the UTF state. */
    (void)pcre2_pattern_info(re, PCRE2_INFO_ALLOPTIONS, &option_bits);
    utf8 = (option_bits & PCRE2_UTF) != 0;

    /* Now find the newline convention and see whether CRLF is a valid newline sequence. */
    (void)pcre2_pattern_info(re, PCRE2_INFO_NEWLINE, &newline);
    crlf_is_newline = newline == PCRE2_NEWLINE_ANY ||
                      newline == PCRE2_NEWLINE_CRLF ||
                      newline == PCRE2_NEWLINE_ANYCRLF;

    /* Loop for second and subsequent matches */
    for (;;) {
        uint32_t options = 0;
        PCRE2_SIZE start_offset = 0;
        if (ovector != NULL) {
            start_offset = ovector[1];    /* Start at end of previous match */

            /* If the previous match was for an empty string, we are finished if we are
            at the end of the subject. Otherwise, arrange to run another match at the
            same point to see if a non-empty match can be found. */
            if (ovector[0] == ovector[1]) {
                if (ovector[0] == subject_length) break;
                options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
            }
        }

        /* Run the next matching operation */
        rc = pcre2_match(
                re,                   /* the compiled pattern */
                subject,              /* the subject string */
                subject_length,       /* the length of the subject */
                start_offset,         /* starting offset in the subject */
                options,              /* options */
                match_data,           /* block for storing the result */
                NULL);                /* use default match context */

        ovector = pcre2_get_ovector_pointer(match_data);
        if (rc == PCRE2_ERROR_NOMATCH) {
            if (options == 0) break;                    /* All matches found */
            ovector[1] = start_offset + 1;              /* Advance one code unit */

            if (crlf_is_newline &&                      /* If CRLF is newline & */
                start_offset < subject_length - 1 &&    /* we are at CRLF, */
                subject[start_offset] == '\r' &&
                subject[start_offset + 1] == '\n') {

                ovector[1] += 1;                          /* Advance by one more. */
            } else if (utf8) {                                         /* advance a whole UTF-8 */
                while (ovector[1] < subject_length) {
                    if ((subject[ovector[1]] & 0xc0) != 0x80) break;
                    ovector[1] += 1;
                }
            }

            continue;
        }

        // Other matching errors are not recoverable.
        if (rc < 0) {
            printf("Matching error %d\n", rc);
            pcre2_match_data_free(match_data);
            pcre2_code_free(re);
            return NULL;
        }

        // Match succeded
        char *single_match_buffer = NULL;
        size_t single_match_buffer_lenght = 0;
        for (i = 0; i < rc; i++) {
            PCRE2_SPTR substring_start = subject + ovector[2*i];
            size_t substring_length = ovector[2*i+1] - ovector[2*i];
            single_match_buffer_lenght += substring_length;

            char *substring_buffer;
            substring_buffer = malloc(substring_length * sizeof(char*));
            sprintf(substring_buffer, "%.*s", (int)substring_length, (char*)substring_start);

            if (single_match_buffer == NULL) {
                single_match_buffer = malloc(single_match_buffer_lenght * sizeof(char*));
                strcpy(single_match_buffer, substring_buffer);
            } else {
                single_match_buffer = realloc(
                        single_match_buffer,
                        single_match_buffer_lenght * sizeof(char*));
                strcat(single_match_buffer, substring_buffer);
            }

            free(substring_buffer);
        }

        final_matches->matches = realloc(
                final_matches->matches,
                (final_matches->n_matches + 1) * sizeof(char*));
        final_matches->matches[final_matches->n_matches] = single_match_buffer;
        final_matches->n_matches++;
    }

    pcre2_match_data_free(match_data);
    pcre2_code_free(re);
    return final_matches;
}

matcher_t* matcher_get_sections(
        const char *content,
        int padding) {
    char section_pattern[100];
    sprintf(section_pattern,
            MATCH_SECTION_PATTERN,
            padding, padding);

    return matcher_match(content, section_pattern);
}

char* matcher_get_section_name(
        const char *section) {
    char *result = NULL;
    matcher_t *section_name;
    section_name = matcher_match(section, MATCH_SECTION_NAME_PATTERN);
    if (section_name->n_matches > 0) {
        result = strdup(section_name->matches[0]);
    }

    free(section_name);
    return result;
}

matcher_t* matcher_get_requires_section(const char *section, int padding) {
    char require_pattern[100];
    sprintf(require_pattern,
            MATCH_SECTION_REQUIRE_PATTERN,
            padding+4);

    return matcher_match(section, require_pattern);
}

char* matcher_get_require_command(const char *require) {
    char *result = NULL;
    matcher_t *section_commands;
    section_commands = matcher_match(require, MATCH_SECTION_COMMAND_PATTERN);
    if (section_commands->n_matches > 0) {
        matcher_t *command;
        command = matcher_match(section_commands->matches[0], MATCH_COMMAND_PATTERN);
        if (command->n_matches > 0) {
            for (int i = 0; i < command->n_matches; ++i) {
                if (result == NULL) {
                    result = malloc(strlen(command->matches[i]) * sizeof(char*));
                    strcpy(result, command->matches[i]);
                } else {
                    result = realloc(
                            result,
                            (strlen(result) + strlen(command->matches[i])) * sizeof(char*));
                    strcat(result, command->matches[i]);
                }
            }
        }
    }

    free(section_commands);
    return result;
}

char* matcher_get_test_name(
        const char *test) {
    char *result = NULL;
    matcher_t *test_name;
    test_name = matcher_match(test, MATCH_TEST_NAME);
    if (test_name->n_matches > 0) {
        result = strdup(test_name->matches[0]);
    }

    free(test_name);
    return result;
}