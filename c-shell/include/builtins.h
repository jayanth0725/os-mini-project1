#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

#define PATH_LENGTH 4096

typedef struct {
    char path[PATH_LENGTH];
    double rank;
    time_t last_visited;
} FrecencyEntry;

void execute_hop(Token *args, const char *shell_home);

void get_frecency_filepath(char *filepath, const char *shell_home);

int load_frecency_data(FrecencyEntry **entries, const char *shell_home);

void save_frecency_data(FrecencyEntry *entries, int count, const char *shell_home);

void execute_reveal(Token *args, const char *shell_home);

#endif