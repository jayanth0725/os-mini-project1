#include "../include/builtins.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#define PATH_LENGTH 4096

// Helper function for qsort that compares two strings using strcmp().
static int compare_strings(const void *a, const void *b){
    const char *str_a = *(const char **)a;
    const char *str_b = *(const char **)b;

    return strcmp(str_a, str_b);
}

// Helper function that updates resolved_path with either the home directory, previous directory or just the target directory.
static int resolve_reveal_path(const char *target, const char *shell_home, char* resolved_path){
    if(strcmp(target, "~") == 0){
        strcpy(resolved_path, shell_home);
        return 1;
    }
    else if(strcmp(target, "-") == 0){
        if(strlen(previous_dir) == 0){
            // There is no previous directory recorded yet for the current execution of the shell.
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

// Helper function that attempts to open the directory, format the entries in the directory according to the flags passed to reveal and the rules of the command.
static void collect_reveal_entries(const char *base_path, const char *relative_prefix, int show_all, int recursive, char ***entries, int *count, int *capacity){
    // Attempts to open the directory, else returns.
    DIR *dir = opendir(base_path);
    if(dir == NULL){
        return;
    }

    // Creates a struct that has all the entries within the opened directory, and iterates through them.
    struct dirent *entry;
    while((entry = readdir(dir)) != NULL){
        // Skips the cwd and parent directory to prevent infinite loop.
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
            continue;
        }

        // Skips the hidden files if the show all flag is not set.
        if(!show_all && entry->d_name[0] == '.'){
            continue;
        }

        // Concatenates the relative and base_path to get the full absolute path.
        char full_path[PATH_LENGTH];
        snprintf(full_path, PATH_LENGTH, "%s/%s", base_path, entry->d_name);

        // Sets the is_dir to 1 if the entry has a valid path and is a directory.
        struct stat st;
        int is_dir = 0;
        if(lstat(full_path, &st) == 0) {
            is_dir = S_ISDIR(st.st_mode);
        }

        // If the recursive flag is set, append '/' to all child directories, and prepend the relative path if available.
        char display_name[PATH_LENGTH];
        if(strlen(relative_prefix) > 0){
            snprintf(display_name, PATH_LENGTH, "%s%s%s", relative_prefix, entry->d_name, (is_dir && recursive) ? "/" : "");            
        }
        else{
            snprintf(display_name, PATH_LENGTH, "%s%s", entry->d_name, (is_dir && recursive) ? "/" : "");
        }

        // If the number of entries exceeds the default capacity, dynamically allocate more memory to entries.
        if(*count >= *capacity){
            *capacity *= 2;
            *entries = realloc(*entries, (*capacity) * sizeof(char*));
        }
        (*entries)[*count] = strdup(display_name);
        (*count)++;

        // The function is called recursively if the recursive flag -t is set.
        if(recursive && is_dir){
            char new_prefix[PATH_LENGTH];
            snprintf(new_prefix, PATH_LENGTH, "%s%s/", relative_prefix, entry->d_name);
            collect_reveal_entries(full_path, new_prefix, show_all, recursive, entries, count, capacity);
        }
    }
    closedir(dir);
}

// The function that actually executes the reveal function and is exposed in the header file.
void execute_reveal(Token *args, const char *shell_home){
    int show_all = 0;
    int recursive = 0;
    char target_path[PATH_LENGTH] = "";
    int target_provided = 0;

    // Iterates through the linked list of Tokens while the tokens are not the operators |, ; or &.
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

        // Stop processing reveal arguments if a non-WORD token (like a redirection or pipe operator) is encountered.
        if(curr->type != WORD){
            break;
        }
        
        // Iterates through the token to set the recursive and show_all flag, and to verify if there are no invalid flags.
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
        // If the argument does not start with '-', treat it as the target path. 
        // If a target was already provided, it means too many arguments were passed, which is a syntax error.
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

    // If the resolve_reveal_path() fails, return to caller.
    char resolved_path[PATH_LENGTH];
    if(!resolve_reveal_path(target_path, shell_home, resolved_path)){
        return;        
    }

    // Attempts to open the directory from resolved_path, if it fails, it prints to stdout and returns to caller.
    DIR *dir = opendir(resolved_path);
    if(dir == NULL){
        printf("reveal: no such directory\n");
        return;
    }
    closedir(dir);

    // Creates an array of reveal entries with a default size of 10.
    int capacity = 10;
    int count = 0;
    char **entries = malloc(capacity * sizeof(char*));

    // Populates the array with entries and recursively traverses subdirectories if the -t flag is set.
    collect_reveal_entries(resolved_path, "", show_all, recursive, &entries, &count, &capacity);

    // The array is sorted lexicographically and printed to stdout.
    qsort(entries, count, sizeof(char*), compare_strings);

    for(int i = 0; i < count; i++){
        printf("%s\n", entries[i]);
    }

    // Each entry is freed, followed by the array itself to prevent memory leaks.
    for(int i = 0; i < count; i++){
        free(entries[i]);
    }
    free(entries);
}