#ifndef ANALYZER_ANALYZER_H
#define ANALYZER_ANALYZER_H

#define START_PADDING 4

void analyzer_recursive_print(
        section_t **sections,
        size_t n_sections);

test_t* analyzer_analyze(
        char *file_path);

int analyzer_recursive_match(
        const char *body,
        int padding,
        test_t *current_test,
        section_t *current_section);

#endif //ANALYZER_ANALYZER_H
