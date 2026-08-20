#include "../include/builtins.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

static int is_line_empty(const char *line){
    for(int i = 0; line[i] != '\0'; i++){
        if(line[i] != ' ' && line[i] != '\t' &&line[i] != '\n' && line[i] != '\r'){
            return 0;
        }
    }
    return 1;
}

static char* join_chunks(const char *part1, int len1, const char *part2, int len2){
    char *res = malloc(len1 + len2 + 1);
    memcpy(res, part1, len1);
    memcpy(res + len1, part2, len2);
    res[len1 + len2] = '\0';
    return res;
}

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

static void peek_file(const char *filename, int show_lines, int reverse){
    if(strcmp(filename, "-") != 0){
        struct stat st;
        if(stat(filename, &st) == 0){
            if(S_ISDIR(st.st_mode)){
                printf("peek: is a directory\n");
                return;
            }
        }
    }

    FILE *file = (strcmp(filename, "-") == 0) ? stdin : fopen(filename, "r");
    if(!file){
        printf("peek: no such file or directory\n");
        return;
    }

    if(reverse){
        int fd = fileno(file);
        off_t file_size = lseek(fd, 0, SEEK_END);

        if(file_size != -1){
            long chunk_size = 4096;
            char *buffer = malloc(chunk_size + 1);
            off_t current_pos = file_size;

            int total_non_empty = 0;
            if(show_lines){
                fseek(file, 0, SEEK_SET);
                char tmp[PATH_LENGTH * 2];
                while(fgets(tmp, sizeof(tmp), file)){
                    if(!is_line_empty(tmp)){
                        total_non_empty++;
                    }
                }
            }

            char *leftover = malloc(1);
            leftover[0] = '\0';
            int leftover_len = 0;

            while(current_pos > 0){
                long bytes_to_read = (current_pos < chunk_size) ? current_pos : chunk_size;
                current_pos -= bytes_to_read;

                lseek(fd, current_pos, SEEK_SET);
                read(fd, buffer, bytes_to_read);
                buffer[bytes_to_read] = '\0';

                int last_boundary = bytes_to_read;

                for(int i = bytes_to_read - 1; i >= 0; i--){
                    if(buffer[i] == '\n'){
                        int segment_len = last_boundary - (i + 1);

                        if(segment_len == 0 && leftover_len == 0 && current_pos + last_boundary == file_size){
                            last_boundary = i + 1;
                            continue;
                        }

                        char *line_to_print = join_chunks(buffer + i + 1, segment_len, leftover, leftover_len);

                        print_peek_line(line_to_print, show_lines, &total_non_empty, 1);

                        free(line_to_print);

                        last_boundary = i + 1;
                        leftover_len = 0;
                        leftover[0] = '\0';
                    }
                }

                int new_seg_len = last_boundary;
                char *new_leftover = join_chunks(buffer, new_seg_len, leftover, leftover_len);
                
                free(leftover);
                leftover = new_leftover;
                leftover_len = new_seg_len + leftover_len;
            }

            if(leftover_len > 0){
                print_peek_line(leftover, show_lines, &total_non_empty, 1);
            }

            free(leftover);
            free(buffer);
        }
        else{
            int capacity = 100;
            int count = 0;
            char **lines = malloc(capacity * sizeof(char*));
            char line[PATH_LENGTH * 2];

            while(fgets(line, sizeof(line), file)){
                if(count >= capacity){
                    capacity *= 2;
                    lines = realloc(lines, capacity * sizeof(char*));
                }
                lines[count++] = strdup(line);
            }

            int total_non_empty = 0;
            if(show_lines){
                for(int i = 0; i < count; i++){                    
                    if(!is_line_empty(lines[i])){
                        total_non_empty++;
                    }
                }
            }

            for(int i = count - 1; i >= 0; i--){
                print_peek_line(lines[i], show_lines, &total_non_empty, 1);
            }

            for(int i=0; i < count; i++){
                free(lines[i]);
            }
            free(lines);
        }
    }
    else{
        char line[PATH_LENGTH * 2];
        int line_num = 1;

        while(fgets(line, sizeof(line), file)){
            print_peek_line(line, show_lines, &line_num, 0);
        }
    }

    if(file != stdin){
        fclose(file);
    }
    else{
        clearerr(stdin);
    }
}

void execute_peek(Token *args){
    int show_lines = 0;
    int reverse = 0;

    Token *curr = args;

    while(curr != NULL && curr->type == WORD){
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

    if(curr == NULL || curr->type != WORD){
        peek_file("-", show_lines, reverse);
        return;
    }

    while(curr != NULL && curr->type == WORD){
        peek_file(curr->value, show_lines, reverse);
        curr = curr->next;
    }
}

