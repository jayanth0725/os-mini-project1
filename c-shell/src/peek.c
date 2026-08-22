#include "../include/builtins.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

// Helper function that checks whether the given line is empty - for counting the lines when -n flag is set.
static int is_line_empty(const char *line){
    for(int i = 0; line[i] != '\0'; i++){
        if(line[i] != ' ' && line[i] != '\t' &&line[i] != '\n' && line[i] != '\r'){
            return 0;
        }
    }
    return 1;
}

// Helper function that joins chunks of the file - for when the -r flag is set.
static char* join_chunks(const char *part1, int len1, const char *part2, int len2){
    char *res = malloc(len1 + len2 + 1);
    memcpy(res, part1, len1);
    memcpy(res + len1, part2, len2);
    res[len1 + len2] = '\0';
    return res;
}

// Helper function that prints the given line. Takes into account if the -n or -r flags are set.
static void print_peek_line(const char *line, int show_lines, int *counter, int is_descending){
    if(show_lines && !is_line_empty(line))    {
        printf("%d %s", is_descending ? (*counter)-- : (*counter)++, line);
    }
    else{
        printf("%s", line);
    }

    if(line[0] != '\0' && line[strlen(line) - 1] != '\n'){
        printf("\n");
    }
}

// Helper function that actually prints to the stdout and accounts for both flags.
static void peek_file(const char *filename, int show_lines, int reverse){
    // If the filename is not '-', it checks whether it is actually a directory, returns immediately if true.
    if(strcmp(filename, "-") != 0){
        struct stat st;
        if(stat(filename, &st) == 0){
            if(S_ISDIR(st.st_mode)){
                printf("peek: is a directory\n");
                return;
            }
        }
    }

    // If filename is '-', sets the file pointer to stdin, else tries to open the given file.
    // Prints error message to stdout and returns immediately if the opening fails.
    FILE *file = (strcmp(filename, "-") == 0) ? stdin : fopen(filename, "r");
    if(!file){
        printf("peek: no such file or directory\n");
        return;
    }

    // This block executes if the reverse flag is set.
    if(reverse){
        // Creates a file descriptor for the file and finds its size.
        int fd = fileno(file);
        off_t file_size = lseek(fd, 0, SEEK_END);

        // If the file is not empty, this block executes.
        if(file_size != -1){
            // Creates a buffer of size 4097 bytes, and a marker for the position in the file.
            long chunk_size = 4096;
            char *buffer = malloc(chunk_size + 1);
            off_t current_pos = file_size;

            // Counts the total number of non-empty lines for the show_lines flag.
            int total_non_empty = 0;
            if(show_lines){
                // Creates a buffer, starts at the beginning and calls is_line_empty() for each line till EOF.
                fseek(file, 0, SEEK_SET);
                char tmp[PATH_LENGTH * 2];
                while(fgets(tmp, sizeof(tmp), file)){
                    if(!is_line_empty(tmp)){
                        total_non_empty++;
                    }
                }
            }

            // Initialises the leftover buffer.
            char *leftover = malloc(1);
            leftover[0] = '\0';
            int leftover_len = 0;

            // Iterates over the file from the end till it reaches 0.
            while(current_pos > 0){
                // Calculates the size of the chunk to read in this iteration, and updates current_pos accordingly.
                long bytes_to_read = (current_pos < chunk_size) ? current_pos : chunk_size;
                current_pos -= bytes_to_read;

                // Move the file pointer to current_pos, read the contents to buffer and end the string with '\0'.
                lseek(fd, current_pos, SEEK_SET);
                read(fd, buffer, bytes_to_read);
                buffer[bytes_to_read] = '\0';

                // Initialises the boundary for reading each line.
                int last_boundary = bytes_to_read;

                // Iterates over the calculated chunk, character by character.
                for(int i = bytes_to_read - 1; i >= 0; i--){
                    // This block executes when '\n' is encountered, which means a line has been completely iterated over.
                    if(buffer[i] == '\n'){
                        // Calculates the length of the line.
                        int segment_len = last_boundary - (i + 1);

                        // If the segment and leftover are both empty, and the pointer is at the very end of the file, skip to avoid printing a blank newline.
                        if(segment_len == 0 && leftover_len == 0 && current_pos + last_boundary == file_size){
                            last_boundary = i + 1;
                            continue;
                        }

                        // Prepend the line segment found in the current chunk to the leftover from the previous chunk (which holds the end of the line).
                        char *line_to_print = join_chunks(buffer + i + 1, segment_len, leftover, leftover_len);

                        // Prints the line to stdout, accounting for the flags.
                        print_peek_line(line_to_print, show_lines, &total_non_empty, 1);

                        free(line_to_print);

                        // Updates the boundary and resets the leftover.
                        last_boundary = i + 1;
                        leftover_len = 0;
                        leftover[0] = '\0';
                    }
                }

                // The remaining beginning of the buffer (before the last found '\n') is prepended to the current leftover to become the new leftover for the next backward chunk.
                int new_seg_len = last_boundary;
                char *new_leftover = join_chunks(buffer, new_seg_len, leftover, leftover_len);

                // Free the old leftover buffer since it has been merged into the new_leftover for the next iteration.
                free(leftover);
                leftover = new_leftover;
                leftover_len = new_seg_len + leftover_len;
            }

            // The leftover here is the beginning line of the file, call print_peek_line.
            if(leftover_len > 0){
                print_peek_line(leftover, show_lines, &total_non_empty, 1);
            }

            // Free the leftover and buffer to prevent memory leaks.
            free(leftover);
            free(buffer);
        }
        // Fallback to print in reverse if the file is non-seekable (pipes, stdin).
        else{
            // Initialises the array of lines, and the line buffer.
            int capacity = 100;
            int count = 0;
            char **lines = malloc(capacity * sizeof(char*));
            char line[PATH_LENGTH * 2];

            // Until EOF is reached, copies each line in the file to lines.
            // It dynamically allocates more memory for lines if the default capacity is exceeded.
            while(fgets(line, sizeof(line), file)){
                if(count >= capacity){
                    capacity *= 2;
                    lines = realloc(lines, capacity * sizeof(char*));
                }
                lines[count++] = strdup(line);
            }

            // Counts the total number of non-empty lines in the file if show_lines flag is set to true.
            int total_non_empty = 0;
            if(show_lines){
                for(int i = 0; i < count; i++){                    
                    if(!is_line_empty(lines[i])){
                        total_non_empty++;
                    }
                }
            }

            // Prints the lines in reverse by calling print_peek_line().
            for(int i = count - 1; i >= 0; i--){
                print_peek_line(lines[i], show_lines, &total_non_empty, 1);
            }

            // Frees each line in the lines array and frees the array itself at the end.
            for(int i=0; i < count; i++){
                free(lines[i]);
            }
            free(lines);
        }
    }
    // This block executes for normal printing without reverse flag.
    else{
        // Initialises the read buffer, and the line counter.
        char line[PATH_LENGTH * 2];
        int line_num = 1;

        // Until EOF is reached, copies each line in the file to line, and calls print_peek_line().
        // It accounts for numbering the lines if applicable.
        while(fgets(line, sizeof(line), file)){
            print_peek_line(line, show_lines, &line_num, 0);
        }
    }

    // If the input for peek was a file, close it.
    // Otherwise, it was stdin, so clear it. 
    if(file != stdin){
        fclose(file);
    }
    else{
        clearerr(stdin);
    }
}

// The function that actually executes the peek function and is exposed in the header file.
void execute_peek(Token *args){
    int show_lines = 0;
    int reverse = 0;

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

        // Stop processing flags if a non-WORD token (like a redirection or pipe operator) is encountered.
        if(curr->type != WORD){
            break;
        }
        
        // Iterates through the token to set the show_lines and reverse flag, and to verify if there are no invalid flags.
        int len = (int)strlen(curr->value);
        if(curr->value[0] == '-' && len > 1){
            for(int i = 1; i < len; i++){
                if(curr->value[i] == 'n'){
                    show_lines = 1;
                }
                else if(curr->value[i] == 'r'){
                    reverse = 1;
                }
                else{
                    printf("peek: invalid syntax\n");
                    return;
                }
            }
        }
        else{
            break;
        }
        curr = curr->next;
    }

    // Either no file is given or only '-' is. This means peek must read from stdin.
    if(curr == NULL || curr->type != WORD){
        peek_file("-", show_lines, reverse);
        return;
    }

    // Secondary loop if there are multiple files in the input.
    while(curr != NULL && curr->type != OP_PIPE && curr->type != OP_SEMI && curr->type != OP_AMP){
        // For handling redirection and piping.
        if(curr->type == OP_LT || curr->type == OP_GT || curr->type == OP_GTGT){
            curr = curr->next;
            if(curr != NULL){
                curr = curr->next;
            }
            continue;
        }

        // Stop processing files if a non-WORD token is encountered.
        if(curr->type != WORD){
            break;
        }
        
        // Prints the contents of the file to stdout, effectively concatenating the contents of the files.
        // Continues to the next file, if any.
        peek_file(curr->value, show_lines, reverse);
        curr = curr->next;
    }
}

