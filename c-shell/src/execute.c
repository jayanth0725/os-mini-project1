#include "../include/execute.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define PATH_LENGTH 4096

static char* check_executable_path(const char *dir, const char *target){
    char full_path[PATH_LENGTH];
    int len = (int)strlen(dir);

    if(len > 0 && dir[len - 1] == '/'){
        snprintf(full_path, PATH_LENGTH, "%s%s", dir, target);
    }
    else{
        snprintf(full_path, PATH_LENGTH, "%s/%s", dir, target);
    }

    struct stat st;
    if(stat(full_path, &st) == 0){
        if(S_ISREG(st.st_mode) && access(full_path, X_OK) == 0){
            return strdup(full_path);
        }
    }

    return NULL;
}

static char* resolve_command_path(const char *cmd){
    if(strchr(cmd, '/') != NULL){
        struct stat st;
        if(stat(cmd, &st) == 0 && S_ISREG(st.st_mode) && access(cmd, X_OK) == 0){
            return strdup(cmd);
        }

        return NULL;
    }

    const char *search_cmd = cmd;
    int skip_cwd = 0;

    if(cmd[0] == '%'){
        search_cmd = cmd + 1;
        skip_cwd = 1;
    }

    if(!skip_cwd){
        char cwd[PATH_LENGTH];
        if(getcwd(cwd, sizeof(cwd)) != NULL){
            char *cwd_path = check_executable_path(cwd, search_cmd);
            if(cwd_path){
                return cwd_path;
            }
        }
    }

    char *path_env = getenv("PATH");
    if(path_env != NULL){
        char *path_copy = strdup(path_env);
        if(path_copy != NULL){
            char *dir = strtok(path_copy, ":");
            while(dir != NULL){
                char *res = check_executable_path(dir, search_cmd);
                if(res){
                    free(path_copy);
                    return res;
                }
                dir = strtok(NULL, ":");
            }
            free(path_copy);
        }
    }

    return NULL;
}

void execute_external(Token *args){
    if(args == NULL || args -> value == NULL){
        return;
    }

    int capacity = 10;
    int argc = 0;
    char **argv = malloc(capacity * sizeof(char*));
    if(!argv){
        perror("Failed to allocate argv");
        return;
    }

    Token *curr = args;
    while(curr != NULL && curr->type == WORD){
        if(argc + 1 >= capacity){
            capacity *= 2;
            argv = realloc(argv, capacity * sizeof(char*));
        }
        argv[argc++] = curr->value;
        curr = curr->next;
    }
    argv[argc] = NULL;

    char *resolved_path = resolve_command_path(argv[0]);
    if(resolved_path == NULL){
        char *cmd_name = argv[0];
        if(cmd_name[0] == '%'){
            cmd_name++;
        }
        printf("cshell: command not found (%s)\n", cmd_name);
        free(argv);
        return;
    }

    pid_t pid = fork();
    if(pid == 0){
        execv(resolved_path, argv);

        perror("execv");
        exit(1);
    }
    else if(pid > 0){
        waitpid(pid, NULL, 0);
    }
    else{
        perror("fork");
    }

    free(resolved_path);
    free(argv);
}