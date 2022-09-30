#ifndef CACHEGRAND_BENCHMARK_GENERATOR_BUILDER_H
#define CACHEGRAND_BENCHMARK_GENERATOR_BUILDER_H

typedef struct section section_t;
struct section {
    char *name;
    int n_subsections;
    section_t **subsections;
    int n_commands;
    char **commands;
};

typedef struct test test_t;
struct test {
    char *name;
    int n_sections;
    section_t **sections;
};

typedef struct tests tests_t;
struct tests {
    int n_tests;
    test_t **tests;
};

section_t* builder_new_section_p();

test_t* builder_new_test_p();

tests_t* builder_new_tests_p();

bool builder_section_append_subsection(
        section_t *section,
        section_t *subsections);

bool builder_section_append_command(
        section_t *section,
        char *command);

bool builder_test_append_section(
        test_t *test,
        section_t *section);

bool builder_tests_append_test(
        tests_t *tests,
        test_t *test);

#endif //CACHEGRAND_BENCHMARK_GENERATOR_BUILDER_H
