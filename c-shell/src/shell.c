#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

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
    }

    return 0;
}