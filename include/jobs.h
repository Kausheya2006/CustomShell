#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_JOBS 20

// NEW: An enum to represent the state of a job.
typedef enum {
    JOB_RUNNING,
    JOB_STOPPED
} JobStatus;

// Represents a background job being tracked by the shell
typedef struct {
    pid_t pgid;                 // The Process Group ID of the job
    int job_id;                 // The shell's job number (e.g., [1], [2])
    char command[1024];         // The original command string
    JobStatus status;           // <-- ADD THIS FIELD to track the state
} Job;

// --- Function Prototypes ---
void init_jobs();
void add_job(pid_t pgid, const char* command_line);
void reap_finished_jobs();
void execute_activities(); // <-- ADD THIS new function prototype

// Part E.3
Job* get_job_by_pgid(pid_t pgid);
void update_job_status(pid_t pgid, JobStatus status);
void kill_all_jobs();

// Part E.4
Job* get_job_by_id(int job_id);
Job* get_most_recent_job();

#endif