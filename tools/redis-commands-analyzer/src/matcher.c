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
#include <pcre2.h>


matcher_t* match(const char *content, const char *regex) {
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
    final_matches->matches = malloc(sizeof(char*));

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

            continue;    /* Go round the loop again */
        }

        /* Other matching errors are not recoverable. */
        if (rc < 0) {
            printf("Matching error %d\n", rc);
            pcre2_match_data_free(match_data);
            pcre2_code_free(re);
            return NULL;
        }

        /* Match succeded */
        char *single_match_buffer = NULL;
        size_t single_match_buffer_lenght = 0;
        for (i = 0; i < rc; i++) {
            PCRE2_SPTR substring_start = subject + ovector[2*i];
            size_t substring_length = ovector[2*i+1] - ovector[2*i];
            single_match_buffer_lenght += substring_length;
    //        printf("%2d: %.*s\n", i, (int)substring_length, (char *)substring_start);

            char *substring_buffer;
            substring_buffer = malloc(substring_length * sizeof(char*));
            sprintf(substring_buffer, "%.*s", (int)substring_length, (char*)substring_start);

            if (single_match_buffer == NULL) {
                single_match_buffer = malloc(single_match_buffer_lenght * sizeof(char*));
                strcpy(single_match_buffer, substring_buffer);
            } else {
                single_match_buffer = realloc(single_match_buffer, single_match_buffer_lenght * sizeof(char*));
                strcat(single_match_buffer, substring_buffer);
            }

            free(substring_buffer);
        }
//        printf("%s", single_match_buffer);

        final_matches->matches = realloc(final_matches->matches, (final_matches->n_matches + 1) * sizeof(char*));
        final_matches->matches[final_matches->n_matches] = single_match_buffer;
        final_matches->n_matches++;
    }

    pcre2_match_data_free(match_data);
    pcre2_code_free(re);
    return final_matches;
}