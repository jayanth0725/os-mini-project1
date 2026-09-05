#include "../include/jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

BackgroundJob bg_jobs[128];

int next_job_id = 1;

int foreground_running = 1;

void init_jobs(){
    for(int i = 0; i < 128; i++){
        bg_jobs[i].is_active = 0;
    }
}

// Registers a newly launched background process.
int add_background_job(pid_t pid, const char *cmd){
    for(int i = 0; i < 128; i++){
        if(!bg_jobs[i].is_active){
            bg_jobs[i].pid = pid;
            bg_jobs[i].job_id = next_job_id++;
            strncpy(bg_jobs[i].command_name, cmd, 255);
            bg_jobs[i].command_name[255] = '\0';
            bg_jobs[i].is_active = 1;
            bg_jobs[i].is_finished = 0;
            return bg_jobs[i].job_id;
        }
    }
    return -1; // Job list full.
}

// The SIGCHLD handler: reaps zombies asynchronously.
void sigchild_handler(int sig){
    int status;
    pid_t pid;

    // Iterate through all the active background jobs.
    for(int i = 0; i < 128; i++){
        if(bg_jobs[i].is_active && !bg_jobs[i].is_finished){
            // Wait only on this specific background process.
            pid = waitpid(bg_jobs[i].pid, &status, WNOHANG);

            if(pid > 0){
                bg_jobs[i].is_finished = 1;
                bg_jobs[i].exit_status = status;

                // If the shell is waiting for user input, print immediately.
                if(!foreground_running){
                    int exited_normally = WIFEXITED(status) && WEXITSTATUS(status) == 0;

                    // snprintf + write is used here because printf is not safe to interrput inside a signal handler.
                    char msg[512];
                    int len = snprintf(msg, sizeof(msg), "%s with pid %d exited %s\n", bg_jobs[i].command_name, pid, exited_normally ? "normally" : "abnormally");
                    write(STDOUT_FILENO, msg, len);

                    bg_jobs[i].is_active = 0; // Marks as fully cleaned up.
                }
            }
        }
    }
}

void setup_sigchild_handler(){
    struct sigaction sa;
    sa.sa_handler = sigchild_handler;
    sigemptyset(&sa.sa_mask);

    // SA_RESTART prevents the signal from interrupting fgets() in the main loop.
    sa.sa_flags = SA_RESTART;

    if(sigaction(SIGCHLD, &sa, NULL) == -1){
        perror("sigaction");
        exit(1);
    }
}

// Called by the main shell after a foreground process finished to print deferred notifications.
void check_background_jobs(){
    for(int i = 0; i < 128; i++){
        if(bg_jobs[i].is_active && bg_jobs[i].is_finished){
            int exited_normally = WIFEXITED(bg_jobs[i].exit_status) && WEXITSTATUS(bg_jobs[i].exit_status) == 0;
            printf("%s with pid %d exited %s\n", bg_jobs[i].command_name, bg_jobs[i].pid,  exited_normally ? "normally" : "abnormally");

            bg_jobs[i].is_active = 0;
        }
    }
}
