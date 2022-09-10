//
// Created by Vito Castellano on 10/09/22.
//
#ifndef ANALYZER_ANALYZER_H
#define ANALYZER_ANALYZER_H

typedef struct command command_t;
struct command {
    char *command;
};

typedef struct section section_t;
struct section {
    char    *name;
    command_t **commands;
};

typedef struct test test_t;
struct test {
    char    *name;
    section_t **sections;
};



#endif //ANALYZER_ANALYZER_H
