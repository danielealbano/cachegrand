//
// Created by Vito Castellano on 10/09/22.
//
#ifndef ANALYZER_ANALYZER_H
#define ANALYZER_ANALYZER_H

void recursive_print(section_t **sections, size_t n_sections);

test_t* read_content(char *file_path);

int recursive_match(const char *body, int padding, test_t *current_test, section_t *current_section);

#endif //ANALYZER_ANALYZER_H
