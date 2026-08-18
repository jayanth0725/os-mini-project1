#include "../include/builtins.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

static int check_executable(const char *dir, const char *target){
    char full_path[PATH_LENGTH];

    int len = (int)strlen(dir);
    if (len > 0 && dir[len-1] == '/'){
        snprintf(full_path, PATH_LENGTH, "%s%s", dir, target);
    }
    else{
        snprintf(full_path, PATH_LENGTH, "%s/%s", dir, target);
    }

    struct stat st;
    if(stat(full_path, &st) == 0){
        if(S_ISREG(st.st_mode) && access(full_path, X_OK) == 0){
            printf("%s\n", full_path);
            return 1;
        }
    }
    return 0;
}

void execute_locate(Token *args){
    if(args == NULL){
        printf("locate: invalid syntax\n");
        return;
    }

    Token *curr = args;
    while(curr != NULL){
        const char *target = curr->value;
        int found = 0;

        char cwd[PATH_LENGTH];
        if(getcwd(cwd, sizeof(cwd)) != NULL){
            if(check_executable(cwd, target)){
                found = 1;
            }
        }

        char *path_env = getenv("PATH");
        if(path_env != NULL){
            char *path_copy = strdup(path_env);
            if(path_copy != NULL){
                char *dir = strtok(path_copy, ":");
                while(dir != NULL){
                    if(check_executable(dir, target)){
                        found = 1;
                    }
                    dir = strtok(NULL, ":");
                }
                free(path_copy);
            }
        }

        if(!found){
            printf("locate: command not found (%s)\n", target);
        }

        curr = curr->next;
    }
}