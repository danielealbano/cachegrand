#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "matcher.h"
#include "builder.h"
#include "support.h"

#include "analyzer.h"

FILE    *current_file;
tests_t *tests;
char    *tests_lists[] ={
//        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-del.cpp",
        "../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-append.cpp"
};


void read_content() {
    char *string = read_file("../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-append.cpp");
//    char *string = read_file("../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-copy.cpp");

//    char *regex = "^\\s{4}SECTION\\(\".*\"\\)\\s{\\n([\\s\\S]*?^\\s{4}\\}\\n)$";
    char *regex = "^\\s{4}SECTION\\(\".*\"\\)\\s{\\n";

    if (string) {
        matcher_t *match_results;
        match_results = match(string, regex);

        for (int i = 0; i < match_results->n_matches; ++i) {
            printf("-------------------------------------------------------------\n");
            printf("%s \n", match_results->matches[i]);
            printf("-------------------------------------------------------------\n");
        }
    }

}

int main() {
    printf("Start main \n");

    // Init tests
    tests = new_tests_p();

    size_t n_tests = sizeof(tests_lists) / sizeof(char*);
    for (int i = 0; i < n_tests; ++i) {
        // Open file to elaborate
        current_file = fopen(tests_lists[i], "rb");
        if (NULL == current_file) {
            printf("file can't be opened \n");
            continue;
        }

        read_content();

        fclose(current_file);
    }

//    for (int i = 0; i < tests->n_tests; ++i) {
//        printf("Test Founded: %s \n", tests->tests[i]->name);
//        for (int j = 0; j < tests->tests[i]->n_sections; ++j) {
//            printf("Section Fouded: %s \n", tests->tests[i]->sections[j]->name);
//            for (int k = 0; k < tests->tests[i]->sections[j]->n_commands; ++k) {
//                printf("Command Fouded: %s \n", tests->tests[i]->sections[j]->commands[k]->command);
//            }
//        }
//    }

    return EXIT_SUCCESS;
}

