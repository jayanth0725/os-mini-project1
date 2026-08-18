#include "../include/parser.h"
#include "../include/builtins.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define PATH_LENGTH 4096
#define HOST_LENGTH 255

void display_prompt(const char *home){
    char *username = getenv("USER");
    if(username == NULL){
        username = getlogin();
    }

    char hostname[HOST_LENGTH];
    if(gethostname(hostname, sizeof(hostname)) != 0){
        strcpy(hostname, "unknown");
    }

    char cwd[PATH_LENGTH];
    if(getcwd(cwd, sizeof(cwd)) == NULL){
        strcpy(cwd, "unknown");
    }

    int home_length = strlen(home);

    if(strncmp(cwd, home, home_length) == 0 && (cwd[home_length] == '\0' || cwd[home_length] == '/')){
        printf("<%s@%s:~%s> ", username, hostname, cwd + home_length);
    }
    else{
        printf("<%s@%s:%s> ", username, hostname, cwd);
    }
}

int main(){
    char shell_home[PATH_LENGTH];
    if(getcwd(shell_home, sizeof(shell_home)) == NULL){
        perror("Failed to get starting directory");
        return 1;
    }

    char input_buffer[PATH_LENGTH];

    while(1){
        display_prompt(shell_home);
        fflush(stdout);

        if(fgets(input_buffer, sizeof(input_buffer), stdin) == NULL){
            printf("\n");
            break;
        }

        input_buffer[strcspn(input_buffer, "\n")] = '\0';

        Token *tokens = tokenise_input(input_buffer);

        if(tokens != NULL){
            if(validate_grammar(tokens)){
                if(tokens->type == WORD){
                    if(strcmp(tokens->value, "hop") == 0){
                        execute_hop(tokens->next, shell_home);
                    }
                    else if(strcmp(tokens->value, "reveal") == 0){
                        execute_reveal(tokens->next, shell_home);
                    }
                    else if(strcmp(tokens->value, "peek") == 0){
                        execute_peek(tokens->next);
                    }
                    else{
                        printf("cshell: command not found (%s)\n", tokens->value);
                    }
                }
            }

            free_tokens(tokens);
        }
    }

    return 0;
}