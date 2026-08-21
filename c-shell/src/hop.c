#include "../include/builtins.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define PATH_LENGTH 4096

char previous_dir[PATH_LENGTH] = "";

static void get_frecency_filepath(char *filepath, const char *shell_home){
    snprintf(filepath, PATH_LENGTH, "%s/.cshell_frecency", shell_home);
}

static int load_frecency_data(FrecencyEntry **entries, const char *shell_home){
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

static void save_frecency_data(FrecencyEntry *entries, int count, const char *shell_home){
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

static double calculate_frecency(double rank, time_t last_visited){
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

static void record_visit(const char *path, const char *shell_home){
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

    int valid_count = 0;
    for(int i = 0; i < count;i++){
        if(access(entries[i].path, F_OK) != 0){
            continue;
        }

        double current_score = calculate_frecency(entries[i].rank, entries[i].last_visited);
        if(current_score < 0.5 && strcmp(entries[i].path, path) != 0){
            continue;
        }

        entries[valid_count] = entries[i];
        valid_count++;
    }

    save_frecency_data(entries, valid_count, shell_home);
    free(entries);
}

static int attempt_frecency_hop(const char *name, const char *shell_home){
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
    while(current_arg != NULL && current_arg->type != OP_PIPE && current_arg->type != OP_SEMI && current_arg->type != OP_AMP){
        if(current_arg->type == OP_LT || current_arg->type == OP_GT || current_arg->type == OP_GTGT){
            current_arg = current_arg->next;
            if(current_arg != NULL){
                current_arg = current_arg->next;
            }
            continue;
        }

        if(current_arg->type != WORD){
            break;
        }
        
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