#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"
#include <time.h>

#define PATH_LENGTH 4096

extern char previous_dir[PATH_LENGTH];

typedef struct {
    char path[PATH_LENGTH];
    double rank;
    time_t last_visited;
} FrecencyEntry;

void execute_hop(Token *args, const char *shell_home);

void execute_reveal(Token *args, const char *shell_home);

void execute_peek(Token *args);

void execute_locate(Token *args);

#endif