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

// Helper function that creates the absolute path of the given file, checks if it exists, and is an executable file.
static char* check_executable_path(const char *dir, const char *target){
    char full_path[PATH_LENGTH];
    int len = (int)strlen(dir);

    // If dir ends with a '/', the full path is created by directly appending target to it.
    // Otherwise, target is appended to dir after adding a '/'.
    if(len > 0 && dir[len - 1] == '/'){
        snprintf(full_path, PATH_LENGTH, "%s%s", dir, target);
    }
    else{
        snprintf(full_path, PATH_LENGTH, "%s/%s", dir, target);
    }

    // If the file actually exists, and it is a regular file with executable permissions, the absolute path is printed to the terminal with successful return value of 1.
    struct stat st;
    if(stat(full_path, &st) == 0){
        if(S_ISREG(st.st_mode) && access(full_path, X_OK) == 0){
            return strdup(full_path);
        }
    }

    return NULL;
}

// Helper function that resolves the path of the executable if it exists either in cwd or elsewhere that is defined in the 'PATH' environment variable.
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

    // If '%' is set, the cwd must not be searched, so skip_cwd is set to 1 and the search_cmd pointer is moved forward by a character.
    if(cmd[0] == '%'){
        search_cmd = cmd + 1;
        skip_cwd = 1;
    }

    // If '%' is not set, searches in the cwd for the given name of the executable and returns the path if successful.
    if(!skip_cwd){
        char cwd[PATH_LENGTH];
        if(getcwd(cwd, sizeof(cwd)) != NULL){
            char *cwd_path = check_executable_path(cwd, search_cmd);
            if(cwd_path){
                return cwd_path;
            }
        }
    }

    // Retrieves the environment variable 'PATH', duplicates it and tokenises it with ':' as the delimiter.
    // Then checks each token to see if it is an executable, returns the first executable path if successful.
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

// Helper function that checks whether token is one of the following builtin commands: hop, reveal, peek or locate.
static int is_builtin(Token *token){
    if(!token || token->type != WORD){
        return 0;
    }
    return (strcmp(token->value, "hop") == 0 || strcmp(token->value, "reveal") == 0 || strcmp(token->value, "peek") == 0 || strcmp(token->value, "locate") == 0);
}

// Helper function that executes external commands other than the builtin ones by calling execv().
static void execute_external(Token *args){
    // If the linked list of Tokens is empty, return immediately.
    if(args == NULL || args -> value == NULL){
        return;
    }

    // Initialise argc and allocate argv with a default of 10 for the execv command.
    // Calls perror() and returns if malloc fails.
    int capacity = 10;
    int argc = 0;
    char **argv = malloc(capacity * sizeof(char*));
    if(!argv){
        perror("Failed to allocate argv");
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

        // If the token is a word, it is added to argv. Dynamically allocates more memory to argv if default capacity is exceeded.
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

    // In case there were no arguments somehow, return immediately.
    if(argc == 0){
        free(argv);
        return;
    }

    // If the resolve_command_path() fails, format the command name based on if '%' was set, print the error message to stdout, and return.
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

    // The path was resolved successfully, call execv and pass argv to it.
    execv(resolved_path, argv);

    // If these lines are executed, this means execv() failed, since it never returns.
    perror("execv");
    exit(1);
}

// The function that actually executes all the command functions and is exposed in the header file.
// It also handles all redirection and piping.
void execute_command_group(Token *tokens, const char *shell_home){
    // Iterates through all the tokens to count the number of pipes in the input.
    int pipe_count = 0;
    Token *scan = tokens;
    while(scan != NULL && scan->type != OP_SEMI && scan->type != OP_AMP){
        if(scan->type == OP_PIPE){
            pipe_count++;
        }
        scan = scan->next;
    }

    // Initialise the components required for piping.
    int num_cmds = pipe_count + 1;
    Token *cmd_start = tokens;
    int prev_pipe[2];
    int curr_pipe[2];
    pid_t pids[128];

    // Save the shell's original standard input and output file descriptors.
    // These will be restored at the end of the pipeline so the shell can read/print normally again.
    int saved_stdin = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);

    // Iterate through all the commands in the input.
    for(int i = 0; i < num_cmds; i++){
        // If this is not the last command in the pipeline, create a new pipe (curr_pipe).
        // curr_pipe[1] will be the write-end for this command, and curr_pipe[0] will be the read-end for the next.
        if(i < num_cmds - 1 && pipe(curr_pipe) < 0){
            perror("pipe");
            break;
        }

        // Initialise the counters and arrays of files for redirection.
        int in_count = 0;
        char *input_files[128];

        int out_count = 0;
        char *out_files[128];
        int out_append[128];

        Token *curr = tokens;

        // Iterate through the list of Tokens and update the arrays of input, output and append output files.
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

        // Checks if the first command is a builtin.
        int builtin = is_builtin(cmd_start);
        pid_t pid = 0;

        // If it is not a builtin, or if it's part of a pipeline, create a child process with fork().
        if(num_cmds > 1 || !builtin){
            pid = fork();
        }

        // Executes this block if it is the child process or there is only one builtin function (runs in parent).
        if(pid == 0 || (num_cmds == 1 && builtin)){
            int exec_failed = 0;

            // For handling pipeline redirection (connecting pipes).
            if(num_cmds > 1){
                // If this is not the first command, read input from the previous command's pipe.
                if(i > 0){
                    dup2(prev_pipe[0], STDIN_FILENO);   // Replace stdin with the read-end of the previous pipe.
                    close(prev_pipe[0]);                // Close original pipe fd since it's now duplicated to stdin.
                    close(prev_pipe[1]);                // Close the write-end; this child only reads from it.
                }
                // If this is not the last command, write output to the current command's pipe.
                if(i < num_cmds - 1){
                    close(curr_pipe[0]);                // Close the read-end; this child only writes to it.
                    dup2(curr_pipe[1], STDOUT_FILENO);  // Replace stdout with the write-end of the current pipe.
                    close(curr_pipe[1]);                // Close original pipe fd since it's now duplicated to stdout.
                }
            }

            // For handling input redirection.
            if(in_count > 0){
                // If there is only one input file, directly open it and duplicate it to stdin.
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
                    // The input consists of multiple files. A temporary file is used to concatenate their contents.
                    FILE *tmp = tmpfile();
                    if(!tmp){
                        perror("tmpfile");
                        exec_failed = 1;
                    }
                    // If the temp file is created successfully, prepare to copy the input files into it.
                    else{
                        int tmp_fd = fileno(tmp);
                        char buffer[4096];
                        // Iterates through each input file to read its contents and write to tmpfile.
                        for(int i = 0; i < in_count; i++){
                            // Attempt to open the input file, flag error and break if it fails.
                            int fd = open(input_files[i], O_RDONLY);
                            if(fd < 0){
                                printf("cshell: no such file or directory\n");
                                exec_failed = 1;
                                break;
                            }
                            ssize_t bytes;
                            // Read the file chunk by chunk and write it into the temporary file.
                            while((bytes = read(fd, buffer, sizeof(buffer))) > 0){
                                write(tmp_fd, buffer, bytes);
                            }
                            close(fd);
                        }

                        // If all inputs were read successfully, rewind the temporary file to the beginning and redirect stdin to it.
                        if(!exec_failed){
                            lseek(tmp_fd, 0, SEEK_SET);
                            dup2(tmp_fd, STDIN_FILENO);
                        }
                        fclose(tmp);
                    }
                }
            }

            // Initialising arrays and fd for output redirection.
            int out_fds[128];
            FILE *out_tmp = NULL;
            int out_tmp_fd = -1;

            if(!exec_failed && out_count > 0){
                // Iterates through each output file to create/open them with proper truncate/append flags.
                for(int i = 0; i < out_count; i++){
                    int flags = O_WRONLY | O_CREAT;
                    flags |= out_append[i] ? O_APPEND : O_TRUNC;

                    // Attempts to open the file with 0644 permissions. Closes any already opened descriptors on failure.
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

                // This executes if the output files were created successfully.
                if(!exec_failed){
                    // If there is only one output file, directly redirect standard output to it.
                    if(out_count == 1){
                        dup2(out_fds[0], STDOUT_FILENO);
                    }
                    // If there are multiple output files and it's a builtin, use a temporary file to capture output.
                    // (External commands handle multi-output later using a proxy pipe).
                    else if(out_count > 1 && builtin){
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
            
            // If there are no failures in redirection and piping, and the Token is a word, its execution is attempted here.
            if(!exec_failed && cmd_start->type == WORD){
                // Checks if it is one of the builtins, else calls execute_external() - must be an external command or an invalid one.
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
                    // For multiple output files with an external command, a multi-output pipe is required.
                    // The files cannot be written to after execv() because the process image is replaced.
                    if(out_count > 1){
                        int multi_out_pipe[2];
                        pipe(multi_out_pipe);
                        pid_t cmd_pid = fork();

                        // If it is the newly forked command process (child 2), redirect standard output to the pipe and execute.
                        if(cmd_pid == 0){
                            dup2(multi_out_pipe[1], STDOUT_FILENO);
                            close(multi_out_pipe[0]);
                            close(multi_out_pipe[1]);
                            execute_external(cmd_start);    // This calls execv and replaces the process.
                            exit(1);                        // Force exit if execv fails (command not found).
                        }
                        // The writer process (the original child 1) reads the command's output from the pipe and writes it to all output files.
                        else{
                            close(multi_out_pipe[1]);
                            char buffer[4096];
                            ssize_t bytes;
                            while((bytes = read(multi_out_pipe[0], buffer, sizeof(buffer))) > 0){
                                for(int i = 0; i < out_count; i++){
                                    write(out_fds[i], buffer, bytes);
                                }
                            }
                            close(multi_out_pipe[0]);
                            waitpid(cmd_pid, NULL, 0);  // Wait for the executed command to finish.
                        }
                    }
                    else{
                        // Standard execution for 0 or 1 output files.
                        execute_external(cmd_start);
                    }
                }
            }

            // For builtins with multiple outputs, flush the tmpfile into the actual output files.
            if(!exec_failed && out_count > 1 && out_tmp != NULL){
                // Flush the input buffer, set the file pointer, create a buffer and write into each of the output files.
                fflush(stdout);
                lseek(out_tmp_fd, 0, SEEK_SET);
                char buffer[4096];
                ssize_t bytes;
                while((bytes = read(out_tmp_fd, buffer, sizeof(buffer))) > 0){
                    for(int i = 0; i < out_count; i++){
                        write(out_fds[i], buffer, bytes);
                    }
                }
                // Close the tmpfile.
                fclose(out_tmp);
            }

            // Close all the destination output files.
            if(!exec_failed && out_count > 0){
                for(int i = 0; i < out_count; i++){
                    close(out_fds[i]);
                }
            }

            // If a child process (external command or pipeline component) is running, exit so the shell is not duplicated.
            if(num_cmds > 1 || !builtin){
                exit(exec_failed ? 1 : 0);
            }
        }

        // Track the child process pid.
        if(num_cmds > 1 || !builtin)
            pids[i] = pid;

        // The parent (shell) must close the previous pipe completely, as the current child has already consumed it.
        // It then moves curr_pipe into prev_pipe so the next command in the loop can read from it.
        if(num_cmds > 1){
            if(i > 0){
                close(prev_pipe[0]);    // Close the read-end of the old pipe.
                close(prev_pipe[1]);    // Close the write-end of the old pipe.
            }

            if(i < num_cmds - 1){
                // Shift the current pipe to become the previous pipe for the next iteration.
                prev_pipe[0] = curr_pipe[0];
                prev_pipe[1] = curr_pipe[1];
            }
        }

        // If this was a single builtin command, it ran in the parent process and modified STDIN/STDOUT directly.
        // The original stdout and stdin must be immediately restored so the shell doesn't break.
        if(num_cmds == 1 && builtin){
            dup2(saved_stdout, STDOUT_FILENO);
            dup2(saved_stdin, STDIN_FILENO);
        }
        
        // Advance cmd_start pointer to the next command in the pipeline.
        while(cmd_start != NULL && cmd_start->type != OP_PIPE && cmd_start->type != OP_SEMI && cmd_start->type != OP_AMP){
            cmd_start = cmd_start->next;
        }

        // Skip the pipe operator itself.
        if(cmd_start != NULL && cmd_start->type == OP_PIPE){
            cmd_start = cmd_start->next;
        }
    }

    // Wait for all child processes in the pipeline to finish executing before returning to the prompt.
    for(int i = 0; i < num_cmds; i++){
        waitpid(pids[i], NULL, 0);
    }

    // Restore the shell's original standard input and output file descriptors globally.
    dup2(saved_stdout, STDOUT_FILENO);
    dup2(saved_stdin, STDIN_FILENO);

    // Close the saved backups since they are no longer needed.
    close(saved_stdout);
    close(saved_stdin);
}