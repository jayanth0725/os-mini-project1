#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include <signal.h>

// Struct to track background processes.
typedef struct {
    pid_t pid;
    int job_id;
    char command_name[256];
    int is_active;
    int is_finished;
    int exit_status;
} BackgroundJob;

// Global flag to track if the shell is currently waiting on a foreground process.
extern int foreground_running;

void init_jobs();

int add_background_job(pid_t pid, const char *cmd);

void setup_sigchild_handler();

void check_background_jobs();

#endif