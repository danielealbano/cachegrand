#ifndef CACHEGRAND_BENCHMARK_GENERATOR_ANALYZER_H
#define CACHEGRAND_BENCHMARK_GENERATOR_ANALYZER_H

#define ANALYZER_START_PADDING 4

test_t* analyzer_analyze(
        const char *file_path);

int analyzer_recursive_match(
        const char *body,
        int padding,
        test_t *current_test,
        section_t *current_section);

#endif //CACHEGRAND_BENCHMARK_GENERATOR_ANALYZER_H
