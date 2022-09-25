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
//    char *string = read_file("../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-append.cpp");
    char *string = read_file("../../../../tests/unit_tests/modules/redis/command/test-modules-redis-command-copy.cpp");

//    char *regex = "^\\s{4}SECTION\\(\".*\"\\)\\s{\\n([\\s\\S]*?^\\s{4}\\}\\n)$";
//    char *regex = "^\\s{4}SECTION\\(\".*\"\\)\\s{\\n";
//    char *regex = "\"([^\"]*)\"";
//    char *regex = "\\\".*\\\"";



//    if (string) {
//        matcher_t *match_results;
//        match_results = match(string, regex);
//
//        for (int i = 0; i < match_results->n_matches; ++i) {
//            printf("-------------------------------------------------------------\n");
//            printf("%s \n", match_results->matches[i]);
//            printf("-------------------------------------------------------------\n");
//        }
//    }


    // Testing recursive
    int starting_padding = 4;
    recursive_match(string, starting_padding);
}

int recursive_match(const char *body, int padding) {
    // Setup regex
    char regex[100];

//    sprintf(regex,"^\\s{%d}SECTION\\(\".*\"\\)\\s{\\n", starting_padding);
//    sprintf(regex,"^\\s{%d}SECTION\\(\".*\"\\)\\s{\\n([\\s\\S]*?^\\s{%d}\\}\\n)$", padding, padding);
    sprintf(regex,"^\\s{%d}SECTION\\(\".*\"\\)\\s{\\n(?:[\\s\\S]*?^\\s{%d}\\}\\n)$", padding, padding);

    // Try to match results
    matcher_t *match_results;
//    match_results = malloc(sizeof(matcher_t));
    match_results = match(body, regex);

    if (match_results->n_matches <= 0) {
        printf("------------------------\n");
        printf("NO OTHERS CHILD FOUND\n");
        printf("------------------------\n");
        free(match_results);
        return 0;
    }

    printf("#############################################\n");
    printf("%d Sections found!\n", match_results->n_matches);
    printf("#############################################\n");
    for (int i = 0; i < match_results->n_matches; ++i) {
        //printf("%s", match_results->matches[i]);

        // Extract section name
        char *pattern_section_name = "\\\".*\\\"";
        matcher_t *section_name;
        section_name = match(match_results->matches[i], pattern_section_name);
        if (section_name->n_matches > 0) {
            printf("%s\n", section_name->matches[0]);
        } else {
            printf("WHHHHHHHATTAAA HELL?!?!?!?\n");
        }
        free(section_name);

        // TODO search commands
        char pattern_require[100];
        sprintf(pattern_require, "^\\s{%d}REQUIRE\\([\\s\\S]*?\\);", padding+4);
        matcher_t *section_requires;
        section_requires = match(match_results->matches[i], pattern_require);
        if (section_requires->n_matches > 0) {
            printf("%d REQUIRE found: \n", section_requires->n_matches);
            for (int j = 0; j < section_requires->n_matches; ++j) {
                //printf("%s\n\n", section_requires->matches[j]);

                // Extract command from require
                char *pattern_command = "(?<=std::vector<std::string>{)[^}]*";
                matcher_t *section_commands;
                section_commands = match(section_requires->matches[j], pattern_command);
                if (section_commands->n_matches > 0) {
                    printf("%d COMMAND found: \n", section_commands->n_matches);
                    for (int k = 0; k < section_commands->n_matches; ++k) {
                        printf("%s\n\n", section_commands->matches[k]);
                    }
                }
                free(section_commands);

            }
        } else {
            printf("No require found for this section\n");
        }
        free(section_requires);


        recursive_match(match_results->matches[i], padding*2);
    }

    free(match_results);
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

