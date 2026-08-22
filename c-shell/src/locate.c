#include "../include/builtins.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

// Helper function that creates the absolute path of the given file, checks if it exists, and is an executable file.
static int check_executable(const char *dir, const char *target){
    char full_path[PATH_LENGTH];

    // If dir ends with a '/', the full path is created by directly appending target to it.
    // Otherwise, target is appended to dir after adding a '/'.
    int len = (int)strlen(dir);
    if (len > 0 && dir[len-1] == '/'){
        snprintf(full_path, PATH_LENGTH, "%s%s", dir, target);
    }
    else{
        snprintf(full_path, PATH_LENGTH, "%s/%s", dir, target);
    }

    // If the file actually exists, and it is a regular file with executable permissions, the absolute path is printed to the terminal with successful return value of 1.
    struct stat st;
    if(stat(full_path, &st) == 0){
        if(S_ISREG(st.st_mode) && access(full_path, X_OK) == 0){
            printf("%s\n", full_path);
            return 1;
        }
    }
    return 0;
}

// The function that actually executes the locate function and is exposed in the header file.
void execute_locate(Token *args){
    if(args == NULL){
        printf("locate: invalid syntax\n");
        return;
    }

    Token *curr = args;
    while(curr != NULL && curr->type != OP_PIPE && curr->type != OP_SEMI && curr->type != OP_AMP){
        // For handling redirection and piping.
        if(curr->type == OP_LT || curr->type == OP_GT || curr->type == OP_GTGT){
            curr = curr->next;
            if(curr != NULL){
                curr = curr->next;
            }
            continue;
        }

        //
        if(curr->type != WORD){
            break;
        }
        
        const char *target = curr->value;
        int found = 0;

        // locate first checks in the cwd if there is an executable with the given name, sets found to 1 if successful.
        char cwd[PATH_LENGTH];
        if(getcwd(cwd, sizeof(cwd)) != NULL){
            if(check_executable(cwd, target)){
                found = 1;
            }
        }

        // Retrieves the environment variable 'PATH', duplicates it and tokenises it with ':' as the delimiter.
        // Then checks each token to see if it is an executable, sets found to 1 if successful.
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

        // If there is no executable with the given name, print this message to stdout.
        if(!found){
            printf("locate: command not found (%s)\n", target);
        }

        // Continue to the next token.
        curr = curr->next;
    }
}