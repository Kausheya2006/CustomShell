#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#include "fg_bg.h"
#include "jobs.h"
#include "main.h" // For access to foreground_pgid

/**
 * @brief Waits for a specific job to either terminate or stop again.
 * This is the core blocking logic for the 'fg' command.
 * @param job A pointer to the job to wait for.
 */
static void wait_for_job(Job* job) {
    int status;
    // We wait specifically for any process within the job's process group.
    // WUNTRACED allows us to also detect if the job is stopped again by Ctrl-Z.
    if (waitpid(-job->pgid, &status, WUNTRACED) > 0) {
        if (WIFSTOPPED(status)) {
            // The job was stopped again.
            job->status = JOB_STOPPED;
            printf("\n[%d] Stopped %s\n", job->job_id, job->command);
        } else {
            // The job terminated. Mark it for removal.
            // reap_finished_jobs will print the "Done" message.
            job->pgid = 0; 
        }
    }
}

/**
 * @brief Implements the 'fg' built-in command.
 */
void execute_fg(char** args) {
    Job* job;
    if (args[1] == NULL) {
        // Requirement: If no job number, use the most recently created job.
        job = get_most_recent_job();
    } else {
        int job_id = atoi(args[1]);
        job = get_job_by_id(job_id);
    }

    if (job == NULL) {
        printf("fg: No such job\n");
        return;
    }

    // Requirement: Print the command being brought to the foreground.
    printf("%s\n", job->command);

    // Give control of the terminal to the job's process group.
    tcsetpgrp(STDIN_FILENO, job->pgid);
    foreground_pgid = job->pgid; // Set global variable for signal handlers

    // Send the "continue" signal to the process group in case it was stopped.
    if (kill(-job->pgid, SIGCONT) < 0) {
        perror("fg: kill (SIGCONT)");
        tcsetpgrp(STDIN_FILENO, getpgrp()); // Take control back on error
        return;
    }

    // Update status to running.
    job->status = JOB_RUNNING;

    // Wait for the job to complete or stop.
    wait_for_job(job);

    // Reset the foreground process tracker.
    foreground_pgid = 0;
    
    // Take back control of the terminal for the shell.
    tcsetpgrp(STDIN_FILENO, getpgrp());
}

/**
 * @brief Implements the 'bg' built-in command.
 */
void execute_bg(char** args) {
    Job* job;
    if (args[1] == NULL) {
        job = get_most_recent_job();
    } else {
        int job_id = atoi(args[1]);
        job = get_job_by_id(job_id);
    }

    if (job == NULL) {
        printf("bg: No such job\n");
        return;
    }

    if (job->status == JOB_RUNNING) {
        printf("bg: Job already running\n");
        return;
    }

    // Update the job's status and send the continue signal.
    job->status = JOB_RUNNING;
    if (kill(-job->pgid, SIGCONT) < 0) {
        perror("bg: kill (SIGCONT)");
        return;
    }

    // Requirement: Print the resume message.
    printf("[%d] %s &\n", job->job_id, job->command);
}