#include "../include/builtins.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define PATH_LENGTH 4096

// Stores the last directory hopped to previously so that 'hop -' will take the shell back to that directory.
char previous_dir[PATH_LENGTH] = "";

// Helper function that updates filepath with the path to the hidden .cshell_frecency file.
static void get_frecency_filepath(char *filepath, const char *shell_home){
    snprintf(filepath, PATH_LENGTH, "%s/.cshell_frecency", shell_home);
}

// Helper function that reads the entries from .cshell_frecency and loads them into an array of Frecency entries after removing the delimiter '|' using string.h functions.
static int load_frecency_data(FrecencyEntry **entries, const char *shell_home){
    char filepath[PATH_LENGTH];
    get_frecency_filepath(filepath, shell_home);

    // Opens the file, if it fails returns to caller with empty entries.
    FILE *file = fopen(filepath, "r");
    if(!file){
        *entries = NULL;
        return 0;
    }

    // By default sets the number of entries to 10.
    int capacity = 10;
    int count = 0;
    *entries = malloc(capacity * sizeof(FrecencyEntry));

    char line[PATH_LENGTH + 256];

    // Reads each line in the file until EOF is reached.
    while(fgets(line, sizeof(line), file)){
        // Dynamically allocates more memory if the number of entries exceed 10.
        if(count >= capacity){
            capacity *= 2;
            *entries = realloc(*entries, capacity * sizeof(FrecencyEntry));
        }

        // Replaces the '\n' at the end of each line with '\0'
        line[strcspn(line, "\n")] = '\0';

        // Finds the memory location in line, where '|' appears from the end of the string.
        // If there is no pipe, the entry is malformed so skip to the next line.
        char *last_pipe = strrchr(line, '|');
        if(!last_pipe)
            continue;
        
        // Finds the memory location in line, where '|' appears from the start of the string. This is the last_visited value and is now separated from rank by '\0'.
        char *first_pipe = strrchr(line, '|');
        *last_pipe = '\0';
        first_pipe = strrchr(line, '|');

        // If there is no first pipe, the entry is malformed so skip to the next line.
        if(!first_pipe)
            continue;

        // This separates the path of the directory from the rank.
        *first_pipe = '\0';

        // Copy the line up to the first '\0', which is the directory path to (*entries)[count].path. Using the markers first_pipe and last_pipe, enter the values of (*entries)[count].rank and (*entries)[count].last_visited.
        strcpy((*entries)[count].path, line);
        (*entries)[count].rank = atof(first_pipe + 1);
        (*entries)[count].last_visited = (time_t)atol(last_pipe + 1);

        count++;
    }

    // Close the file and return the number of entries.
    fclose(file);
    return count;
}

// Helper function that writes the updated frecency entries back into .cshell_frecency.
static void save_frecency_data(FrecencyEntry *entries, int count, const char *shell_home){
    char filepath[PATH_LENGTH];
    get_frecency_filepath(filepath, shell_home);

    FILE *file = fopen(filepath, "w");
    if(!file){
        perror("Failed to open frecency file for writing");
        return;
    }

    // Writes each entry from entries in the format "%s|%f|%ld\n" into the file.
    for(int i=0;i < count; i++){
        fprintf(file, "%s|%f|%ld\n", entries[i].path, entries[i].rank, (long)entries[i].last_visited);
    }

    fclose(file);
}

// Helper function that calculates the frecency of an entry based on its rank and last_visited values.
static double calculate_frecency(double rank, time_t last_visited){
    // now has the number of seconds since the UNIX Epoch from now.
    // Elapsed is the time difference from last visit for the directory and the present time.
    time_t now = time(NULL);
    double elapsed = difftime(now, last_visited);

    // Increases and decreases the frecency of the entry according to the rules for Zoxide.
    if(elapsed < 3600){
        // If the last hop to the directory was within an hour.
        return rank * 4.0;
    }
    else if(elapsed < 86400){
        // If the last hop to the directory was within a day.
        return rank * 2.0;
    }
    else if(elapsed < 604800){
        // If the last hop to the directory was within a week.
        return rank * 0.5;
    }
    else{
        // If the last hop to the directory was more than a week ago.
        return rank * 0.25;
    }
}

// Helper function that builds the array of entries and updates the entry of the directory that was hopped to.
// Also skips entries that no longer exist or whose rank < 0.5.
static void record_visit(const char *path, const char *shell_home){

    // Builds the array of entries.
    FrecencyEntry *entries = NULL;
    int count = load_frecency_data(&entries, shell_home);

    // Updates the entry of the directory that was hopped to and sets found to 1.
    int found = 0;
    for(int i = 0; i < count; i++){
        if(strcmp(entries[i].path, path) == 0){
            entries[i].rank += 1.0;
            entries[i].last_visited = time(NULL);
            found = 1;
            break;
        }
    }

    // If the entry was not found, it means it is the first time it is being hopped to, so the entry is appended to entries.
    if(!found){
        entries = realloc(entries, (count + 1) * sizeof(FrecencyEntry));
        strcpy(entries[count].path, path);
        entries[count].rank = 1.0;
        entries[count].last_visited = time(NULL);
        count++;
    }

    // Skips those entries which either no longer exist or the rank < 0.5.
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

    // Writes the updated entries back to .cshell_frecency and frees the array of entries.
    save_frecency_data(entries, valid_count, shell_home);
    free(entries);
}

// Helper function that attempts to hop valid best match for the entered directory in the frecency table.
// Returns 1 if it is successful, else 0.
static int attempt_frecency_hop(const char *name, const char *shell_home){
    FrecencyEntry *entries = NULL;
    int count = load_frecency_data(&entries, shell_home);
    if(count == 0){
        return 0;
    }

    int best_index = -1;
    double best_score = -1.0;

    // Iterates through all the entries, updating the best_index to the entry which is a valid path (the entered path is a substring of it) and has the highest frecency.
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

    // If a valid entry was found, it attempts to change the cwd to it.
    // success records whether the directory change succeeded.
    int success = 0;
    if(best_index != -1){
        if(chdir(entries[best_index].path) == 0){
            success = 1;
        }
    }

    free(entries);
    return success;
}

// The function that actually executes the hop function and is exposed in the header file.
void execute_hop(Token *args, const char *shell_home){
    // If the linked list of Tokens is empty, executes this block.
    // This means that hop must change the cwd to the directory where the shell started - its 'home directory'.
    if(args == NULL){
        // Gets the path of the cwd and saves it to old_dir.
        char old_dir[PATH_LENGTH];
        getcwd(old_dir, sizeof(old_dir));

        // Attempts to change directory to the 'home directory'.
        // Otherwise, calls perror().
        if(chdir(shell_home) == 0){
            // Updates the value of previous_dir with the directory it just came from.
            strcpy(previous_dir, old_dir);

            // Saves the path of cwd to new_dir, and records the visit in .cshell_frecency.
            char new_dir[PATH_LENGTH];
            getcwd(new_dir, sizeof(new_dir));
            record_visit(new_dir, shell_home);
        }
        else{
            perror("hop");
        }
        return;
    }

    // Iterates through the linked list of Tokens.
    Token *current_arg = args;
    while(current_arg != NULL && current_arg->type != OP_PIPE && current_arg->type != OP_SEMI && current_arg->type != OP_AMP){
        // For handling redirection and piping.
        if(current_arg->type == OP_LT || current_arg->type == OP_GT || current_arg->type == OP_GTGT){
            current_arg = current_arg->next;
            if(current_arg != NULL){
                current_arg = current_arg->next;
            }
            continue;
        }

        // Stop processing hop arguments if a non-WORD token is encountered (e.g., unexpected tokens).
        if(current_arg->type != WORD){
            break;
        }
        
        // Gets the path of the cwd and saves it to old_dir.
        char old_dir[PATH_LENGTH];
        getcwd(old_dir, sizeof(old_dir));

        // Sets the target directory value to target, and initialises the success flag.
        const char* target = current_arg->value;
        int success = 0;

        // Attempts to change to the 'home directory' if the target is '~', sets success to 1 if succeeds.
        if(strcmp(target, "~") == 0){
            success = (chdir(shell_home) == 0);
        }
        // Otherwise, if the target is '-', it attempts to hop to previous_dir.
        // If length of previous_dir = 0, this is the first time hop was called, so print error to stdout and continue.
        else if(strcmp(target, "-") == 0){
            if(strlen(previous_dir) == 0){
                printf("hop: no previous directory\n");
                current_arg = current_arg->next;
                continue;
            }
            success = (chdir(previous_dir) == 0);
        }
        // The target was set to the cwd, so nothing needs to be done. success has automatically succeeded.
        else if(strcmp(target, ".") == 0){
            success = 1;
        }
        // Otherwise, it falls back to a frecency lookup since the name hasn’t resolved directly.
        else {
            success = (chdir(target) == 0);

            if(!success){
                success = attempt_frecency_hop(target, shell_home);
                if(!success){
                    printf("hop: no such directory\n");
                }
            }
        }

        // If the hop was a success, the previous_dir is updated, and the entry of the cwd is updated in .cshell_frecency.
        if(success){
            strcpy(previous_dir, old_dir);

            char new_dir[PATH_LENGTH];
            getcwd(new_dir, sizeof(new_dir));
            record_visit(new_dir, shell_home);
        }

        // Continue to the next Token.
        current_arg = current_arg->next;
    }

    return;
}