#ifndef CACHEGRAND_BENCHMARK_GENERATOR_MATCHER_H
#define CACHEGRAND_BENCHMARK_GENERATOR_MATCHER_H

/* The PCRE2_CODE_UNIT_WIDTH macro must be defined before including pcre2.h.
For a program that uses only one code unit width, setting it to 8, 16, or 32
makes it possible to use generic function names such as pcre2_compile(). */
#define PCRE2_CODE_UNIT_WIDTH 8

typedef struct matches matcher_t;
struct matches {
    int n_matches;
    char **matches;
};

matcher_t* matcher_match(
        const char *content,
        const char *regex);

matcher_t* matcher_get_sections(
        const char *content,
        int padding);

char* matcher_get_section_name(
        const char *section);

matcher_t* matcher_get_requires_section(
        const char *section,
        int padding);

char* matcher_get_require_command(
        const char *require);

char* matcher_get_test_name(
        const char *test);

#endif //CACHEGRAND_BENCHMARK_GENERATOR_MATCHER_H
