#include "../include/parser.h"
#include "../include/builtins.h"
#include "../include/execute.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

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
                    int saved_stdin = dup(STDIN_FILENO);
                    int saved_stdout = dup(STDOUT_FILENO);

                    int in_count = 0;
                    char *input_files[128];

                    int out_count = 0;
                    char *out_files[128];
                    int out_append[128];

                    Token *curr = tokens;

                    while(curr != NULL && curr->type != OP_PIPE && curr->type != OP_SEMI && curr->type != OP_AMP){
                        if(curr->type == OP_LT && curr->next != NULL && curr->next->type == WORD){
                            input_files[in_count++] = curr->next->value;
                        }
                        else if(curr->type == OP_GT && curr->next != NULL && curr->next->type == WORD){
                            out_append[out_count] = 0;
                            out_files[out_count++] = curr->next->value;                            
                        }
                        else if(curr->type == OP_GTGT && curr->next != NULL && curr->next->type == WORD){
                            out_append[out_count] = 1;
                            out_files[out_count++] = curr->next->value;                            
                        }
                        curr = curr->next;
                    }

                    int exec_failed = 0;
                    if(in_count > 0){
                        if(in_count == 1){
                            int fd = open(input_files[0], O_RDONLY);
                            if(fd < 0){
                                printf("cshell: no such file or directory\n");
                                exec_failed = 1;
                            }
                            else{
                                dup2(fd, STDIN_FILENO);
                                close(fd);
                            }
                        }
                        else{
                            FILE *tmp = tmpfile();
                            if(!tmp){
                                perror("tmpfile");
                                exec_failed = 1;
                            }
                            else{
                                int tmp_fd = fileno(tmp);
                                char buffer[4096];
                                for(int i = 0; i < in_count; i++){
                                    int fd = open(input_files[i], O_RDONLY);
                                    if(fd < 0){
                                        printf("cshell: no such file or directory\n");
                                        exec_failed = 1;
                                        break;
                                    }
                                    ssize_t bytes;
                                    while((bytes = read(fd, buffer, sizeof(buffer))) > 0){
                                        write(tmp_fd, buffer, bytes);
                                    }
                                    close(fd);
                                }

                                if(!exec_failed){
                                    lseek(tmp_fd, 0, SEEK_SET);
                                    dup2(tmp_fd, STDIN_FILENO);
                                }
                                fclose(tmp);
                            }
                        }
                    }

                    int out_fds[128];
                    FILE *out_tmp = NULL;
                    int out_tmp_fd = -1;

                    if(!exec_failed && out_count > 0){
                        for(int i = 0; i < out_count; i++){
                            int flags = O_WRONLY | O_CREAT;
                            flags |= out_append[i] ? O_APPEND : O_TRUNC;

                            out_fds[i] = open(out_files[i], flags, 0644);
                            if(out_fds[i] < 0){
                                printf("cshell: unable to create file for writing\n");
                                exec_failed = 1;

                                for(int j = 0; j < i; j++){
                                    close(out_fds[j]);
                                }
                                break;
                            }
                        }

                        if(!exec_failed){
                            if(out_count == 1){
                                dup2(out_fds[0], STDOUT_FILENO);
                            }
                            else{
                                out_tmp = tmpfile();
                                if(!out_tmp){
                                    perror("tmpfile");
                                    exec_failed = 1;
                                    for(int i = 0; i < out_count; i++){
                                        close(out_fds[i]);
                                    }
                                }
                                else{
                                    out_tmp_fd = fileno(out_tmp);
                                    dup2(out_tmp_fd, STDOUT_FILENO);
                                }
                            }
                        }
                    }
                    
                    if(!exec_failed){
                        if(strcmp(tokens->value, "hop") == 0){
                            execute_hop(tokens->next, shell_home);
                        }
                        else if(strcmp(tokens->value, "reveal") == 0){
                            execute_reveal(tokens->next, shell_home);
                        }
                        else if(strcmp(tokens->value, "peek") == 0){
                            execute_peek(tokens->next);
                        }
                        else if(strcmp(tokens->value, "locate") == 0){
                            execute_locate(tokens->next);
                        }
                        else{
                            execute_external(tokens);
                        }
                    }

                    dup2(saved_stdout, STDOUT_FILENO);
                    close(saved_stdout);

                    if(!exec_failed && out_count > 0){
                        if(out_count > 1 && out_tmp != NULL){
                            lseek(out_tmp_fd, 0, SEEK_SET);
                            char buffer[4096];
                            ssize_t bytes;

                            while((bytes = read(out_tmp_fd, buffer, sizeof(buffer))) > 0){
                                for(int i =0; i < out_count; i++){
                                    write(out_fds[i], buffer, bytes);
                                }
                            }
                            fclose(out_tmp);
                        }

                        for(int i = 0; i < out_count; i++){
                            close(out_fds[i]);
                        }
                    }
                    
                    dup2(saved_stdin, STDIN_FILENO);
                    close(saved_stdin);
                }
            }

            free_tokens(tokens);
        }
    }

    return 0;
}