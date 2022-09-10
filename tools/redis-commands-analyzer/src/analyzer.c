#include "stdio.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "support.h"

#include "analyzer.h"

int foundedSection = 0;
char *sections[] = {};

///////////////////////////////////////////////////////////

section_t* new_section_p() {
    section_t *section;
    section = malloc(sizeof(section_t));
    return section;
}

//TODO
//bool section_append_command(
//        command_t *command,
//        int n_section,
//        section_t *section) {
//    test->sections = realloc(test->sections, n_section+1 * sizeof(section_t));
//    test->sections[n_section] = section;
//    if (test->sections[n_section] != NULL) {
//        return true;
//    }
//
//    return false;
//}

///////////////////////////////////////////////////////////

test_t* new_test_p() {
    test_t *test;
    test = malloc(sizeof(test_t));
    test->sections = (section_t**) malloc(1 * sizeof(section_t));
    return test;
}

bool test_append_section(
        test_t *test,
        int n_section,
        section_t *section) {
    test->sections = realloc(test->sections, n_section+1 * sizeof(section_t));
    test->sections[n_section] = section;
    if (test->sections[n_section] != NULL) {
        return true;
    }

    return false;
}

void test_free_sections(
        test_t *test,
        int n_section) {
    for (int i = 0; i < n_section; ++i) {
        free(test->sections[i]);
    }
}

///////////////////////////////////////////////////////////

char** pattern_matcher(
        char *pattern,
        int nmatch,
        char *line) {
    char **result;
    char *str;
    regex_t regex;
    regmatch_t *match;

    if (0 != (regcomp(&regex, pattern, REG_EXTENDED))) {
        printf("regcomp() failed, returning nonzero (%s)\n", pattern);
        exit(EXIT_FAILURE);
    }

    str = strdup(line);
    match = malloc(nmatch * sizeof(*match));
    if (0 != (regexec(&regex,str, nmatch,match,0))) {
        return NULL;
    }

    result = malloc(sizeof(*result));
    if (!result) {
        fprintf(stderr, "Out of memory !");
        return NULL;
    }

    for (int i = 0; i < nmatch; ++i) {
        ((char**) result)[i] = "";
        if (match[i].rm_so >= 0) {
            str[match[i].rm_eo] = 0;
            ((char**)result)[i] = str + match[i].rm_so;
            // printf("%s\n", string + match[i].rm_so);
        }
    }

    return result;
}

bool is_testcase(
        char *line) {
    char **res;
    char *pattern = "TEST_CASE_METHOD";
    int  nmatch   = 1;

    res = pattern_matcher(pattern, nmatch, line);
    if (res) {
        return true;
    }

    return false;
}

char* get_testcase_name(
        char *line) {
    char **res;
    char *pattern = "\"([^\"]*)\"";
    int  nmatch   = 2;

    res = pattern_matcher(pattern, nmatch, line);
    if (res) {
        printf("Test name: %s\n\n", res[nmatch-1]);
        return res[nmatch-1];
    }

    return NULL;
}

char* get_section_name(
        char *line) {
    char **res;
    char *pattern = "\"([^\"]*)\"";
    int  nmatch   = 2;

    res = pattern_matcher(pattern, nmatch, line);
    if (res) {
        printf("Section name: %s\n", res[nmatch-1]);
        return res[nmatch-1];
    }

    return NULL;
}

bool is_section(
        char *line) {
    char **res;
    char *pattern = "SECTION";
    int  nmatch   = 1;

    res = pattern_matcher(pattern, nmatch, line);
    if (res) {
        ++foundedSection;
        return true;
    }

    return false;
}

bool is_command(
        char *line) {
    char **res;
    char *pattern = "std::vector<std::string>";
    int  nmatch   = 1;

    res = pattern_matcher(pattern, nmatch, line);
    if (res) {
        return true;
    }

    return false;
}

char* get_command(
        char *line) {
    char **res;
    char *pattern = "\"(.*?)\"";
    int  nmatch   = 1;

    res = pattern_matcher(pattern, nmatch, line);

    char *command = NULL;
    command = malloc(500);
    if (res) {
        for (int i = 0; i < nmatch; ++i) {
            strcat(command,res[i]);
        }
    }

    //TODO: make it better
    command = replace_char(command, '"', ' ');
    command = replace_char(command, ',', ' ');

    printf("Command: %s\n", command);
    return command;
}

void read_content() {
    FILE *file;
    char line[500];

//    file = fopen("../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-append.cpp", "r");
    file = fopen("../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-del.cpp", "r");
    if (NULL == file) {
        printf("file can't be opened \n");
    }

    test_t **tests;
    tests = (test_t**) malloc(1 * sizeof(test_t));


    int n_section = 0;
    section_t *current_section = new_section_p();
    test_t *current_test;
    while (fgets(line, 500, file) != NULL) {
        // Check if is a new test case
        if (is_testcase(line)) {
            current_test = new_test_p();
            if (current_test->sections == NULL) {
                printf("error test sections memory allocator");
                exit(EXIT_FAILURE);
            }

            current_test->name = get_testcase_name(line);
        }

        // Check if is a new section
        if (is_section(line)) {
            printf("---- NEW SECTION ---- \n");
            // Get Section Name
            char *section_name;
            section_name = get_section_name(line);

            // Check if section is differnte than previous
            if (section_name != current_section->name && current_section->name) {
                if (!test_append_section(
                        current_test,
                        n_section,
                        current_section)) {
                    printf("memory error appending sections");
                    exit(EXIT_FAILURE);
                }

                // TODO: if not present any test in this section probabily is a child

                // Init new section
                current_section = new_section_p();
                ++n_section;
            }

            // Save section information
            current_section->name = section_name;
        }

        // If pass this probabily we have a section
        if (is_command(line) && current_section->name) {
//            command_t *command;
//            commandData = get_command(line);

            get_command(line);
        }
    }

    // Append last section and free pointer
    if (!test_append_section(
            current_test,
            n_section,
            current_section)) {
        printf("memory error appending sections");
        exit(EXIT_FAILURE);
    }

    fclose(file);
    tests[0] = current_test;

    for (int i = 0; i <= n_section; ++i) {
        printf("QUIIIi %s", tests[0]->sections[i]->name);
    }

    test_free_sections(current_test, n_section);
    free(tests);
}

int main() {
    printf("Start main \n");

    read_content();

//    printf("SECTIONS FOUD: %u \n", foundedSection);
//    for (int i = 0; i < foundedSection; ++i) {
//        printf("%s", sections[i]);
//
//        free(sections[i]);
//    }

    return EXIT_SUCCESS;
}

