#include "../include/builtins.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#define PATH_LENGTH 4096

static int compare_strings(const void *a, const void *b){
    const char *str_a = *(const char **)a;
    const char *str_b = *(const char **)b;

    return strcmp(str_a, str_b);
}

static int resolve_reveal_path(const char *target, const char *shell_home, char* resolved_path){
    if(strcmp(target, "~") == 0){
        strcpy(resolved_path, shell_home);
        return 1;
    }
    else if(strcmp(target, "-") == 0){
        if(strlen(previous_dir) == 0){
            printf("reveal: no such directory\n");
            return 0;
        }
        strcpy(resolved_path, previous_dir);
        return 1;
    }
    else{
        strcpy(resolved_path, target);
        return 1;
    }
}

static void collect_reveal_entries(const char *base_path, const char *relative_prefix, int show_all, int recursive, char ***entries, int *count, int *capacity){
    DIR *dir = opendir(base_path);
    if(dir == NULL){
        return;
    }

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
            continue;
        }

        if(!show_all && entry->d_name[0] == '.'){
            continue;
        }

        char full_path[PATH_LENGTH];
        snprintf(full_path, PATH_LENGTH, "%s/%s", base_path, entry->d_name);

        struct stat st;
        int is_dir = 0;
        if(stat(full_path, &st) == 0) {
            is_dir = S_ISDIR(st.st_mode);
        }

        char display_name[PATH_LENGTH];
        if(strlen(relative_prefix) > 0){
            snprintf(display_name, PATH_LENGTH, "%s%s%s", relative_prefix, entry->d_name, (is_dir && recursive) ? "/" : "");            
        }
        else{
            snprintf(display_name, PATH_LENGTH, "%s%s", entry->d_name, (is_dir && recursive) ? "/" : "");
        }

        if(*count >= *capacity){
            *capacity *= 2;
            *entries = realloc(*entries, (*capacity) * sizeof(char*));
        }
        (*entries)[*count] = strdup(display_name);
        (*count)++;

        if(recursive && is_dir){
            char new_prefix[PATH_LENGTH];
            snprintf(new_prefix, PATH_LENGTH, "%s%s/", relative_prefix, entry->d_name);
            collect_reveal_entries(full_path, new_prefix, show_all, recursive, entries, count, capacity);
        }
    }
    closedir(dir);
}

void execute_reveal(Token *args, const char *shell_home){
    int show_all = 0;
    int recursive = 0;
    char target_path[PATH_LENGTH] = "";
    int target_provided = 0;

    Token *curr = args;
    while(curr != NULL && curr->type == WORD){
        int len = (int)strlen(curr->value);
        if(curr->value[0] == '-' && len > 1){
            for(int i = 1; i < len; i++){
                if(curr->value[i] == 'a'){
                    show_all = 1;
                }
                else if(curr->value[i] == 't'){
                    recursive = 1;
                }
                else{
                    printf("reveal: invalid syntax\n");
                    return;
                }
            }
        }
        else{
            if(target_provided){
                printf("reveal: invalid syntax\n");
                return;
            }
            strcpy(target_path, curr->value);
            target_provided = 1;
        }
        curr = curr->next;
    }

    if(!target_provided){
        strcpy(target_path, ".");
    }

    char resolved_path[PATH_LENGTH];
    if(!resolve_reveal_path(target_path, shell_home, resolved_path)){
        return;        
    }

    DIR *dir = opendir(resolved_path);
    if(dir == NULL){
        printf("reveal: no such directory\n");
        return;
    }
    closedir(dir);

    int capacity = 10;
    int count = 0;
    char **entries = malloc(capacity * sizeof(char*));

    collect_reveal_entries(resolved_path, "", show_all, recursive, &entries, &count, &capacity);

    qsort(entries, count, sizeof(char*), compare_strings);

    for(int i = 0; i < count; i++){
        printf("%s\n", entries[i]);
    }

    for(int i = 0; i < count; i++){
        free(entries[i]);
    }
    free(entries);
}