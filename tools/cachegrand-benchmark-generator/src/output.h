#ifndef CACHEGRAND_BENCHMARK_GENERATOR_OUTPUT_H
#define CACHEGRAND_BENCHMARK_GENERATOR_OUTPUT_H

void output_stdout_print(
        section_t **sections,
        size_t n_sections);

void output_json_builder(
        section_t **sections,
        size_t n_sections,
        json_object *j_sections_array);

void output_json_print(
        tests_t *test_collections);

#endif //CACHEGRAND_BENCHMARK_GENERATOR_OUTPUT_H
