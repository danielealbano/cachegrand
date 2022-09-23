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


int match(const char *content, const char *regex) {
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
    PCRE2_SIZE *ovector;

    size_t subject_length;
    pcre2_match_data *match_data;

    pattern = (PCRE2_SPTR)regex;
    subject = (PCRE2_SPTR)content;
    subject_length = strlen((char *)subject);

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
        return 1;
    }

    /***********************
    * MATCH DATA
    ************************/
    match_data = pcre2_match_data_create_from_pattern(re, NULL);

    rc = pcre2_match(
            re,                   /* the compiled pattern */
            subject,              /* the subject string */
            subject_length,       /* the length of the subject */
            0,                    /* start at offset 0 in the subject */
            0,                    /* default options */
            match_data,           /* block for storing the result */
            NULL);                /* use default match context */

    // Catch error
    if (rc < 0) {
        switch(rc) {
            case PCRE2_ERROR_NOMATCH: printf("No match\n"); break;
            default: printf("Matching error %d\n", rc); break;
        }

        pcre2_match_data_free(match_data);
        pcre2_code_free(re);
        return 1;
    }

    // Match succeded
    ovector = pcre2_get_ovector_pointer(match_data);
    printf("--------------------------------------------------------------------------\n");
    printf("Match succeeded at offset %d\n", (int)ovector[0]);

    // Show substrings stored in the output vector by number.
    for (i = 0; i < rc; i++) {
        PCRE2_SPTR substring_start = subject + ovector[2*i];
        size_t substring_length = ovector[2*i+1] - ovector[2*i];
        printf("%2d: %.*s\n", i, (int)substring_length, (char *)substring_start);
    }

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
        uint32_t options = 0;                    /* Normally no options */
        PCRE2_SIZE start_offset = ovector[1];    /* Start at end of previous match */

        /* If the previous match was for an empty string, we are finished if we are
        at the end of the subject. Otherwise, arrange to run another match at the
        same point to see if a non-empty match can be found. */
        if (ovector[0] == ovector[1]) {
            if (ovector[0] == subject_length) break;
            options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
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
            return 1;
        }

        /* Match succeded */
        printf("--------------------------------------------------------------------------\n");
        printf("Match succeeded 2 again at offset %d\n", (int)ovector[0]);

        /* As before, show substrings stored in the output vector by number, and then
        also any named substrings. */
        for (i = 0; i < rc; i++) {
            PCRE2_SPTR substring_start = subject + ovector[2*i];
            size_t substring_length = ovector[2*i+1] - ovector[2*i];
            printf("%2d: %.*s\n", i, (int)substring_length, (char *)substring_start);
        }
    }      /* End of loop to find second and subsequent matches */

    printf("\n");
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);
    return 0;
}