#ifndef CACHEGRAND_BENCHMARK_GENERATOR_OUTPUT_H
#define CACHEGRAND_BENCHMARK_GENERATOR_OUTPUT_H

#if DEBUG == 1
void output_stdout_print(
        section_t **sections,
        size_t n_sections);
#endif

void output_json_builder(
        section_t **sections,
        size_t n_sections,
        json_object *j_sections_array);

void output_json(
        tests_t *test_collections,
        char* output_file_path);

#endif //CACHEGRAND_BENCHMARK_GENERATOR_OUTPUT_H
