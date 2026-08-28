#include "../include/parser.h"
#include "../include/builtins.h"
#include "../include/execute.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define PATH_LENGTH 4096                // POSIX compliant path length.
#define HOST_LENGTH 255                 // POSIX compliant host name length.

// Prints the prompt to the stdout in the format <username@hostname: absolute/relative path>.
void display_prompt(const char *home){
    // Gets the username from the environment variable 'USER'.
    char *username = getenv("USER");
    if(username == NULL){
        username = getlogin();          // Fallback function in case getenv() fails.
    }

    // gethostname() retrieves the host name.
    // But if it returns a non-zero value, hostname gets the value 'unknown'.
    char hostname[HOST_LENGTH];
    if(gethostname(hostname, sizeof(hostname)) != 0){
        strcpy(hostname, "unknown");
    }

    // getcwd() retrieves the current working directory (CWD).
    // But if it returns a non-zero value, cwd also gets the value 'unknown'.
    char cwd[PATH_LENGTH];
    if(getcwd(cwd, sizeof(cwd)) == NULL){
        strcpy(cwd, "unknown");
    }

    int home_length = strlen(home);

    // If the cwd is not the shell's starting directory or a child of it, the path remains absolute.
    // Otherwise, it is made relative to '~'. 
    if(strncmp(cwd, home, home_length) == 0 && (cwd[home_length] == '\0' || cwd[home_length] == '/')){
        printf("<%s@%s:~%s> ", username, hostname, cwd + home_length);
    }
    else{
        printf("<%s@%s:%s> ", username, hostname, cwd);
    }
}

int main(){
    // getcwd() here gets the directory from where the shell started running.
    // If it fails, calls perror() and stops the shell.
    char shell_home[PATH_LENGTH];
    if(getcwd(shell_home, sizeof(shell_home)) == NULL){
        perror("Failed to get starting directory");
        return 1;
    }

    char input_buffer[PATH_LENGTH];

    // Infinite loop to continuously accept input from the user.
    while(1){
        display_prompt(shell_home);
        fflush(stdout);         // Flushes the input buffer to ensure the shell prompt is always printed.

        // Reads a line of input. Stops the shell if EOF (Ctrl+D) is received.
        if(fgets(input_buffer, sizeof(input_buffer), stdin) == NULL){
            printf("\n");
            break;
        }

        // Gets the index of the \n at the end of the input and replaces it with a '\0'.
        input_buffer[strcspn(input_buffer, "\n")] = '\0';

        // Sends the buffer to the lexer to split into tokens.
        Token *tokens = tokenise_input(input_buffer);

        // If the number of tokens is >= 1, continues to the parser.
        if(tokens != NULL){
            // Checks if the input is syntactically correct.
            if(validate_grammar(tokens)){
                Token *curr_group = tokens;

                while(curr_group != NULL){
                    // Execute the current group.
                    if(curr_group->type == WORD){
                        int success = execute_command_group(curr_group, shell_home);
                        if(!success){
                            break;  // Command failed, stop executing the sequence.
                        }
                    }

                    // Advance curr_group to find the operator that ended this command.
                    while(curr_group != NULL && curr_group->type != OP_SEMI && curr_group->type != OP_AMP){
                        curr_group = curr_group->next;
                    }

                    // Move past the operator to the start of the next command group.
                    if(curr_group != NULL){
                        // TokenType op = curr_group->type;
                        curr_group = curr_group->next;
                    }
                }
                
            }
            // Free the tokens of this input to avoid memory leaks.
            free_tokens(tokens);
        }
    }

    return 0;
}