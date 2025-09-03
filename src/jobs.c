#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "jobs.h"
#include <signal.h>

// --- Global Variables ---
Job job_table[MAX_JOBS];
int next_job_id = 1;

void init_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        job_table[i].pgid = 0;
    }
}

// Adds a new job to the table
void add_job(pid_t pgid, const char* command_line) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].pgid == 0) { // Find an empty slot
            job_table[i].pgid = pgid;
            job_table[i].job_id = next_job_id++;
            strncpy(job_table[i].command, command_line, sizeof(job_table[i].command) - 1);
            job_table[i].command[sizeof(job_table[i].command) - 1] = '\0';

            job_table[i].status = JOB_RUNNING;
            // Per requirements, print the job ID and process ID
            printf("[%d] %d\n", job_table[i].job_id, job_table[i].pgid);
            return;
        }
    }
    fprintf(stderr, "shell: Error: too many background jobs.\n");
}

// Checks for any completed background jobs and prints their status
void reap_finished_jobs() {
    int status;
    pid_t pid;

    // waitpid with WNOHANG checks for any terminated child without blocking the shell.
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAX_JOBS; i++) {
            if (job_table[i].pgid == pid) {
                if (WIFEXITED(status)) {
                    printf("%s with pid %d exited normally\n", job_table[i].command, pid);
                } else {
                    printf("%s with pid %d exited abnormally\n", job_table[i].command, pid);
                }
                job_table[i].pgid = 0; // Mark the slot as free
                break;
            }
        }
    }
}


// --- NEW FUNCTION IMPLEMENTATION ---

// This is a helper function for qsort. It compares two Job structs
// based on their command string for lexicographical sorting.
static int compare_jobs_by_name(const void* a, const void* b) {
    Job* jobA = (Job*)a;
    Job* jobB = (Job*)b;
    return strcmp(jobA->command, jobB->command);
}

/**
 * @brief Implements the 'activities' built-in command.
 * It lists all currently running or stopped background jobs, sorted by name.
 */
void execute_activities() {
    // First, clean up any jobs that might have finished since the last prompt.
    reap_finished_jobs();

    // Create a temporary array to hold copies of the active jobs for sorting.
    Job active_jobs[MAX_JOBS];
    int count = 0;

    // Collect all active jobs from the global job table.
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].pgid != 0) { // A non-zero pgid means the slot is in use.
            active_jobs[count++] = job_table[i];
        }
    }

    if (count == 0) {
        return; // Nothing to print if no jobs are active.
    }

    // Sort the temporary array of active jobs lexicographically by command name.
    qsort(active_jobs, count, sizeof(Job), compare_jobs_by_name);

    // Print the sorted list in the required format.
    for (int i = 0; i < count; i++) {
        // Determine the string representation of the job's state.
        const char* state_str = (active_jobs[i].status == JOB_RUNNING) ? "Running" : "Stopped";
        
        // The format is: [pid] : command_name - State
        printf("[%d] : %s - %s\n", 
               active_jobs[i].pgid, 
               active_jobs[i].command, 
               state_str);
    }
}

/**
 * @brief Finds a job in the table by its Process Group ID.
 * @return A pointer to the job, or NULL if not found.
 */
Job* get_job_by_pgid(pid_t pgid) {
    if (pgid < 1) return NULL;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].pgid == pgid) {
            return &job_table[i];
        }
    }
    return NULL;
}

/**
 * @brief Updates the status of a job.
 */
void update_job_status(pid_t pgid, JobStatus status) {
    Job* job = get_job_by_pgid(pgid);
    if (job) {
        job->status = status;
    }
}

/**
 * @brief Sends SIGKILL to all active background jobs. Called on Ctrl-D.
 */
void kill_all_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].pgid != 0) {
            kill(-job_table[i].pgid, SIGKILL);
        }
    }
}

/**
 * @brief Finds a job in the table by its user-facing job ID (e.g., [1], [2]).
 * @return A pointer to the job, or NULL if not found.
 */
Job* get_job_by_id(int job_id) {
    if (job_id <= 0) return NULL;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].job_id == job_id) {
            return &job_table[i];
        }
    }
    return NULL;
}

/**
 * @brief Finds the most recently created job.
 * This is determined by finding the job with the highest job_id.
 * @return A pointer to the job, or NULL if no jobs exist.
 */
Job* get_most_recent_job() {
    int max_id = -1;
    Job* recent_job = NULL;
    for (int i = 0; i < MAX_JOBS; i++) {
        // Find the active job with the largest ID
        if (job_table[i].pgid != 0 && job_table[i].job_id > max_id) {
            max_id = job_table[i].job_id;
            recent_job = &job_table[i];
        }
    }
    return recent_job;
}