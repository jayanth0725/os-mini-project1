#include "../include/execute.h"
#include "../include/builtins.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

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

static int is_builtin(Token *token){
    if(!token || token->type != WORD){
        return 0;
    }
    return (strcmp(token->value, "hop") == 0 || strcmp(token->value, "reveal") == 0 || strcmp(token->value, "peek") == 0 || strcmp(token->value, "locate") == 0);
}

static void execute_external(Token *args){
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
    while(curr != NULL && curr->type != OP_PIPE && curr->type != OP_SEMI && curr->type != OP_AMP){
        if(curr->type == OP_LT || curr->type == OP_GT || curr->type == OP_GTGT){
            curr = curr->next;
            if(curr != NULL){
                curr = curr->next;
            }
            continue;
        }

        if(curr->type == WORD){
            if(argc + 1 >= capacity){
                capacity *= 2;
                argv = realloc(argv, capacity * sizeof(char*));
            }
            argv[argc++] = curr->value;
        }
        
        curr = curr->next;
    }
    argv[argc] = NULL;

    if(argc == 0){
        free(argv);
        return;
    }

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

    execv(resolved_path, argv);

    perror("execv");
    exit(1);
}

void execute_command_group(Token *tokens, const char *shell_home){
    int pipe_count = 0;
    Token *scan = tokens;
    while(scan != NULL && scan->type != OP_SEMI && scan->type != OP_AMP){
        if(scan->type == OP_PIPE){
            pipe_count++;
        }
        scan = scan->next;
    }

    int num_cmds = pipe_count + 1;
    Token *cmd_start = tokens;
    int prev_pipe[2];
    int curr_pipe[2];
    pid_t pids[128];

    int saved_stdin = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);

    for(int i = 0; i < num_cmds; i++){
        if(i < num_cmds - 1 && pipe(curr_pipe) < 0){
            perror("pipe");
            break;
        }

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

        int builtin = is_builtin(cmd_start);
        pid_t pid = 0;

        if(num_cmds > 1 || !builtin){
            pid = fork();
        }

        if(pid == 0 || (num_cmds == 1 && builtin)){
            int exec_failed = 0;

            if(num_cmds > 1){
                if(i > 0){
                    dup2(prev_pipe[0], STDIN_FILENO);
                    close(prev_pipe[0]);
                    close(prev_pipe[1]);
                }
                if(i < num_cmds - 1){
                    close(curr_pipe[0]);
                    dup2(curr_pipe[1], STDOUT_FILENO);
                    close(curr_pipe[1]);
                }
            }

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
            
            if(!exec_failed && cmd_start->type == WORD){
                if(strcmp(cmd_start->value, "hop") == 0){
                    execute_hop(cmd_start->next, shell_home);
                }
                else if(strcmp(cmd_start->value, "reveal") == 0){
                    execute_reveal(cmd_start->next, shell_home);
                }
                else if(strcmp(cmd_start->value, "peek") == 0){
                    execute_peek(cmd_start->next);
                }
                else if(strcmp(cmd_start->value, "locate") == 0){
                    execute_locate(cmd_start->next);
                }
                else{
                    execute_external(cmd_start);
                }
            }

            if(!exec_failed && out_count > 1 && out_tmp != NULL){
                fflush(stdout);
                lseek(out_tmp_fd, 0, SEEK_SET);
                char buffer[4096];
                ssize_t bytes;
                while((bytes = read(out_tmp_fd, buffer, sizeof(buffer))) > 0){
                    for(int i = 0; i < out_count; i++){
                        write(out_fds[i], buffer, bytes);
                    }
                }
                fclose(out_tmp);
            }

            if(!exec_failed && out_count > 0){
                for(int i = 0; i < out_count; i++){
                    close(out_fds[i]);
                }
            }

            if(num_cmds > 1 || !builtin){
                exit(exec_failed ? 1 : 0);
            }
        }

        if(num_cmds > 1 || !builtin)
            pids[i] = pid;

        if(num_cmds > 1){
            if(i > 0){
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }

            if(i < num_cmds - 1){
                prev_pipe[0] = curr_pipe[0];
                prev_pipe[1] = curr_pipe[1];
            }
        }

        if(num_cmds == 1 && builtin){
            dup2(saved_stdout, STDOUT_FILENO);
            dup2(saved_stdin, STDIN_FILENO);
        }
        
        while(cmd_start != NULL && cmd_start->type != OP_PIPE && cmd_start->type != OP_SEMI && cmd_start->type != OP_AMP){
            cmd_start = cmd_start->next;
        }

        if(cmd_start != NULL && cmd_start->type == OP_PIPE){
            cmd_start = cmd_start->next;
        }
    }

    for(int i = 0; i < num_cmds; i++){
        waitpid(pids[i], NULL, 0);
    }

    dup2(saved_stdout, STDOUT_FILENO);
    dup2(saved_stdin, STDIN_FILENO);

    close(saved_stdout);
    close(saved_stdin);
}