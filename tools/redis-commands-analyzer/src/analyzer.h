#ifndef ANALYZER_ANALYZER_H
#define ANALYZER_ANALYZER_H

#define START_PADDING 4

void anlyzer_recursive_print(
        section_t **sections,
        size_t n_sections);

test_t* anlyzer_analyze(
        char *file_path);

int anlyzer_recursive_match(
        const char *body,
        int padding,
        test_t *current_test,
        section_t *current_section);

#endif //ANALYZER_ANALYZER_H
