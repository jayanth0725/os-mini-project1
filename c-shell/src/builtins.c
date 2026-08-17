#include "../include/builtins.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#define PATH_LENGTH 4096

static char previous_dir[PATH_LENGTH] = "";

void get_frecency_filepath(char *filepath, const char *shell_home){
    snprintf(filepath, PATH_LENGTH, "%s/.cshell_frecency", shell_home);
}

int load_frecency_data(FrecencyEntry **entries, const char *shell_home){
    char filepath[PATH_LENGTH];
    get_frecency_filepath(filepath, shell_home);

    FILE *file = fopen(filepath, "r");
    if(!file){
        *entries = NULL;
        return 0;
    }

    int capacity = 10;
    int count = 0;
    *entries = malloc(capacity * sizeof(FrecencyEntry));

    char line[PATH_LENGTH + 256];

    while(fgets(line, sizeof(line), file)){
        if(count >= capacity){
            capacity *= 2;
            *entries = realloc(*entries, capacity * sizeof(FrecencyEntry));
        }

        line[strcspn(line, "\n")] = '\0';

        char *last_pipe = strrchr(line, '|');
        if(!last_pipe)
            continue;
        
        char *first_pipe = strrchr(line, '|');
        *last_pipe = '\0';
        first_pipe = strrchr(line, '|');

        if(!first_pipe)
            continue;

        *first_pipe = '\0';

        strcpy((*entries)[count].path, line);
        (*entries)[count].rank = atof(first_pipe + 1);
        (*entries)[count].last_visited = (time_t)atol(last_pipe + 1);

        count++;
    }

    fclose(file);
    return count;
}

void save_frecency_data(FrecencyEntry *entries, int count, const char *shell_home){
    char filepath[PATH_LENGTH];
    get_frecency_filepath(filepath, shell_home);

    FILE *file = fopen(filepath, "w");
    if(!file){
        perror("Failed to open frecency file for writing");
        return;
    }

    for(int i=0;i < count; i++){
        fprintf(file, "%s|%f|%ld\n", entries[i].path, entries[i].rank, (long)entries[i].last_visited);
    }

    fclose(file);
}

double calculate_frecency(double rank, time_t last_visited){
    time_t now = time(NULL);
    double elapsed = difftime(now, last_visited);

    if(elapsed < 3600){
        return rank * 4.0;
    }
    else if(elapsed < 86400){
        return rank * 2.0;
    }
    else if(elapsed < 604800){
        return rank * 0.5;
    }
    else{
        return rank * 0.25;
    }
}

void record_visit(const char *path, const char *shell_home){
    FrecencyEntry *entries = NULL;
    int count = load_frecency_data(&entries, shell_home);

    int found = 0;
    for(int i = 0; i < count; i++){
        if(strcmp(entries[i].path, path) == 0){
            entries[i].rank += 1.0;
            entries[i].last_visited = time(NULL);
            found = 1;
            break;
        }
    }

    if(!found){
        entries = realloc(entries, (count + 1) * sizeof(FrecencyEntry));
        strcpy(entries[count].path, path);
        entries[count].rank = 1.0;
        entries[count].last_visited = time(NULL);
        count++;
    }

    save_frecency_data(entries, count, shell_home);
    free(entries);
}

int attempt_frecency_hop(const char *name, const char *shell_home){
    FrecencyEntry *entries = NULL;
    int count = load_frecency_data(&entries, shell_home);
    if(count == 0){
        return 0;
    }

    int best_index = -1;
    double best_score = -1.0;

    for(int i=0; i < count; i++){
        if(strstr(entries[i].path, name) != NULL){
            double current_score = calculate_frecency(entries[i].rank, entries[i].last_visited);

            if(current_score > best_score){
                if(access(entries[i].path, F_OK) == 0){
                    best_score = current_score;
                    best_index = i;
                }
            }
        }
    }

    int success = 0;
    if(best_index != -1){
        if(chdir(entries[best_index].path) == 0){
            success = 1;
        }
    }

    free(entries);
    return success;
}

void execute_hop(Token *args, const char *shell_home){
    if(args == NULL){
        char old_dir[PATH_LENGTH];
        getcwd(old_dir, sizeof(old_dir));

        if(chdir(shell_home) == 0){
            strcpy(previous_dir, old_dir);

            char new_dir[PATH_LENGTH];
            getcwd(new_dir, sizeof(new_dir));
            record_visit(new_dir, shell_home);
        }
        else{
            perror("hop");
        }
        return;
    }

    Token *current_arg = args;
    while(current_arg != NULL){
        char old_dir[PATH_LENGTH];
        getcwd(old_dir, sizeof(old_dir));

        const char* target = current_arg->value;
        int success = 0;

        if(strcmp(target, "~") == 0){
            success = (chdir(shell_home) == 0);
        }
        else if(strcmp(target, "-") == 0){
            if(strlen(previous_dir) == 0){
                printf("hop: no previous directory\n");
                current_arg = current_arg->next;
                continue;
            }
            success = (chdir(previous_dir) == 0);
        }
        else if(strcmp(target, ".") == 0){
            success = 1;
        }
        else {
            success = (chdir(target) == 0);

            if(!success){
                success = attempt_frecency_hop(target, shell_home);
                if(!success){
                    printf("hop: no such directory\n");
                }
            }
        }

        if(success){
            strcpy(previous_dir, old_dir);

            char new_dir[PATH_LENGTH];
            getcwd(new_dir, sizeof(new_dir));
            record_visit(new_dir, shell_home);
        }

        current_arg = current_arg->next;
    }

    return;
}

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
    while(curr != NULL){
        size_t len = strlen(curr->value);
        if(curr->value[0] == '-' && len > 1){
            for(size_t i = 1; i < len; i++){
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