#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

// Custom Headers
#include "command.h"
#include "hop.h"
#include "reveal.h"
#include "log.h"
#include "jobs.h"
#include "ping.h"
#include "main.h"

#include "fg_bg.h"

// --- Add the forward declarations with the others ---
void execute_fg(char** args);
void execute_bg(char** args);

// Forward declaration for the functions in your other files
void execute_hop(char** args);
void execute_reveal(char** args);
void execute_ping(char** args); // <-- ADD THIS

// void execute_log(char** args);


/**
 * @brief Executes a single command within a child process. This function is called after fork().
 * It handles I/O redirection and then executes the command, which can be a built-in or external.
 * This function does not return.
 */
void execute_child_command(SimpleCommand* cmd) {

    // Restore the default signal behaviors for the child process.
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);

    // 1. Handle Input Redirection
    // If a file is specified, read its content and append to the command's arguments.
    if (cmd->input_file) {
        int in_fd = open(cmd->input_file, O_RDONLY);
        if (in_fd < 0) {
            perror("shell: input file");
            exit(EXIT_FAILURE);
        }

        char file_content[4096]; // A reasonably sized buffer to hold the file's content
        ssize_t bytes_read = read(in_fd, file_content, sizeof(file_content) - 1);
        close(in_fd);

        if (bytes_read < 0) {
            perror("shell: read input file");
            exit(EXIT_FAILURE);
        }
        file_content[bytes_read] = '\0'; // Ensure the buffer is null-terminated

        // Tokenize the file content by whitespace and append to args
        char* token = strtok(file_content, " \t\n\r");
        while (token != NULL) {
            if (cmd->arg_count < MAX_ARGS - 1) {
                cmd->args[cmd->arg_count] = strdup(token);
                if (cmd->args[cmd->arg_count] == NULL) {
                    perror("shell: strdup"); // Handle memory allocation failure
                    exit(EXIT_FAILURE);
                }
                cmd->arg_count++;
            } else {
                fprintf(stderr, "shell: Too many arguments from input file '%s'.\n", cmd->input_file);
                break; // Stop processing if the argument list is full
            }
            token = strtok(NULL, " \t\n\r");
        }
        cmd->args[cmd->arg_count] = NULL; // Re-terminate the argument list
    }

    // 2. Handle Output Redirection
    // If a file is specified, it overrides any piped output.
    if (cmd->output_file) {
        int out_fd;
        int flags = O_WRONLY | O_CREAT;
        if (cmd->append_mode) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        out_fd = open(cmd->output_file, flags, 0644);
        if (out_fd < 0) {
            perror("shell: output file");
            exit(EXIT_FAILURE);
        }
        if (dup2(out_fd, STDOUT_FILENO) < 0) {
            perror("shell: dup2 stdout");
            exit(EXIT_FAILURE);
        }
        close(out_fd);
    }

    // 3. Execute the command
    if (strcmp(cmd->args[0], "hop") == 0) {
        execute_hop(cmd->args);
        // IMPORTANT: This will change the directory of the CHILD process only.
        // The parent shell's directory will NOT be affected.
        exit(EXIT_SUCCESS);
    } else if (strcmp(cmd->args[0], "reveal") == 0) {
        execute_reveal(cmd->args);
        exit(EXIT_SUCCESS);
    } else if (strcmp(cmd->args[0], "exit") == 0) {
        // This will exit the child process, not the main shell.
        exit(0);
    }
    else if (strcmp(cmd->args[0], "log") == 0) { // <-- THIS WAS ADDED
        // 'log' is now treated as a special built-in.
        execute_log(cmd->args);
        exit(EXIT_SUCCESS);
    }
    else {
        execvp(cmd->args[0], cmd->args);
        // If execvp returns, it means an error occurred.
        fprintf(stderr, "shell: command not found: %s\n", cmd->args[0]);
        exit(EXIT_FAILURE);
    }
    // If it's not a known built-in, assume it's an external command.
    // In your case, since only built-ins are available, we print an error.
    fprintf(stderr, "Error: Command '%s' not found.\n", cmd->args[0]);
    exit(EXIT_FAILURE);
    
    // If you were to add external commands, you would use execvp here:
    // execvp(cmd->args[0], cmd->args);
    // perror("shell: execvp"); // This line only runs if execvp fails
    // exit(EXIT_FAILURE);
}


void execute_pipeline(CommandPipeline* pipeline, const char* original_command) {
    if (pipeline == NULL || pipeline->num_commands == 0) {
        return; // Nothing to execute
    }

    // --- SPECIAL CASE: Handle commands that MUST run in the parent process ---
    // This applies ONLY if it's a single command with no pipes.
    if (pipeline->num_commands == 1) {
        SimpleCommand* cmd = &pipeline->commands[0];
        if (strcmp(cmd->args[0], "hop") == 0) {
            // 'hop' MUST change the parent shell's directory.
            // It can handle input redirection by reading args from a file first.
            if (cmd->input_file) {
                int in_fd = open(cmd->input_file, O_RDONLY);
                if (in_fd < 0) {
                    perror("shell: input file");
                    return; // Return, don't exit the shell
                }

                char file_content[4096];
                ssize_t bytes_read = read(in_fd, file_content, sizeof(file_content) - 1);
                close(in_fd);

                if (bytes_read < 0) {
                    perror("shell: read input file");
                    return;
                }
                file_content[bytes_read] = '\0';

                char* token = strtok(file_content, " \t\n\r");
                while (token != NULL) {
                    if (cmd->arg_count < MAX_ARGS - 1) {
                        cmd->args[cmd->arg_count] = strdup(token);
                        cmd->arg_count++;
                    } else {
                        fprintf(stderr, "shell: Too many arguments from input file '%s'.\n", cmd->input_file);
                        break;
                    }
                    token = strtok(NULL, " \t\n\r");
                }
                cmd->args[cmd->arg_count] = NULL;
            }
            // for (int i = 0; cmd->args[i] != NULL; i++) {
            //     printf("arg[%d]: %s\n", i, cmd->args[i]); // Debugging line
            // }
            execute_hop(cmd->args);
            return; // Command is handled, so we skip the forking logic below.
        } 
        else if (strcmp(cmd->args[0], "exit") == 0) {
            exit(0); // Exit the main shell.
        }
        else if (strcmp(cmd->args[0], "log") == 0) { // <-- THIS WAS ADDED
            // 'log' is now treated as a special built-in.
            execute_log(cmd->args);
            return; // Command is handled, so we skip the forking logic below.
        }
        else if (strcmp(cmd->args[0], "reveal") == 0) {
            // 'reveal' can run in the child process, but we handle it here for consistency.
            execute_reveal(cmd->args);
            return; // Command is handled, so we skip the forking logic below.
        }
        else if (strcmp(cmd->args[0], "activities") == 0) {
            // 'activities' is a built-in that lists background jobs.
            execute_activities();
            return; // Command is handled, so we skip the forking logic below.
        }
        else if (strcmp(cmd->args[0], "ping") == 0) { // <-- ADD THIS BLOCK
            // 'ping' is a simple built-in that sends a signal.
            execute_ping(cmd->args);
            return; // Command is handled.
        }
        else if (strcmp(cmd->args[0], "fg") == 0) { // <-- ADD THIS
            execute_fg(cmd->args);
            return;
        } else if (strcmp(cmd->args[0], "bg") == 0) { // <-- ADD THIS
            execute_bg(cmd->args);
            return;
        }
    }

    // --- GENERAL PIPELINE EXECUTION ---
    // All commands, including 'hop' when in a pipeline, are handled here.
    int num_pipes = pipeline->num_commands - 1;
    pid_t pids[pipeline->num_commands];
    int input_fd = STDIN_FILENO; // The first command reads from stdin

    pid_t pgid = 0; // Process Group ID for the entire pipeline

    for (int i = 0; i < pipeline->num_commands; i++) {
        SimpleCommand* cmd = &pipeline->commands[i];
        int pipe_fds[2];

        // Create a pipe for all but the last command
        if (i < num_pipes) {
            if (pipe(pipe_fds) < 0) {
                perror("shell: pipe");
                return;
            }
        }

        pids[i] = fork();
        if (pids[i] < 0) {
            perror("shell: fork");
            return;
        }

        if (pids[i] == 0) {
            // --- Child Process ---

            pgid = (pgid == 0) ? getpid() : pgid;
            setpgid(0, pgid);

            // Connect input from the previous command
            if (input_fd != STDIN_FILENO) {
                if (dup2(input_fd, STDIN_FILENO) < 0) {
                    perror("shell: dup2 stdin pipe");
                    exit(EXIT_FAILURE);
                }
                close(input_fd);
            }

            // Connect output to the next command
            if (i < num_pipes) {
                close(pipe_fds[0]); // Child doesn't read from the new pipe
                if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) {
                    perror("shell: dup2 stdout pipe");
                    exit(EXIT_FAILURE);
                }
                close(pipe_fds[1]);
            }

            // Now, handle file redirections and execute the command.
            // This will override the pipe redirections if files are specified.
            execute_child_command(cmd);
        }

        // --- Parent Process ---

        pgid = (pgid == 0) ? pids[i] : pgid;
        setpgid(pids[i], pgid);

        // Close the previous pipe's read end
        if (input_fd != STDIN_FILENO) {
            close(input_fd);
        }

        // For the next iteration, the input will be the read end of the current pipe
        if (i < num_pipes) {
            close(pipe_fds[1]); // Parent doesn't write to the new pipe
            input_fd = pipe_fds[0];
        }
    }

    if (pipeline->mode == FOREGROUND) {
        // 1. Set the global foreground pgid so signal handlers know who to target.
        foreground_pgid = pgid;
        
        // 2. Wait for the job to either terminate or stop.
        for (int i = 0; i < pipeline->num_commands; i++) {
            int status;
            // WUNTRACED makes waitpid return if a process is stopped (Ctrl-Z).
            waitpid(pids[i], &status, WUNTRACED);

            // Check if the process was stopped by a signal (Ctrl-Z).
            if (WIFSTOPPED(status)) {
                Job* job = get_job_by_pgid(pgid);
                if (job == NULL) {
                    // It's a new job that needs to be added to the table.
                    add_job(pgid, original_command);
                    job = get_job_by_pgid(pgid);
                }
                if (job) {
                    // Update its status and print the required message.
                    job->status = JOB_STOPPED;
                    printf("\n[%d] Stopped %s\n", job->job_id, job->command);
                }
                break; // Stop waiting for other processes in this job.
            }
        }
        
        // 3. Reset the foreground pgid. No job is in the foreground anymore.
        foreground_pgid = 0;
        
    } 
    else {
        // This is the new behavior for BACKGROUND jobs:
        // DO NOT WAIT. Instead, add the job to our tracking table.
        add_job(pgid, original_command);
    }

    // // Wait for all child processes to complete
    // for (int i = 0; i < pipeline->num_commands; i++) {
    //     int status;
    //     waitpid(pids[i], &status, 0);
    // }
}

