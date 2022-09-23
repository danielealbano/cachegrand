//
// Created by Vito Castellano on 11/09/22.
//

#ifndef ANALYZER_MATCHER_H
#define ANALYZER_MATCHER_H

/* The PCRE2_CODE_UNIT_WIDTH macro must be defined before including pcre2.h.
For a program that uses only one code unit width, setting it to 8, 16, or 32
makes it possible to use generic function names such as pcre2_compile(). */
#define PCRE2_CODE_UNIT_WIDTH 8

typedef struct matches matcher_t;
struct matches {
    int n_matches;
    char **matches;
};

matcher_t* match(
        const char *content,
        const char *regex);


#endif //ANALYZER_MATCHER_H
