#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h> 

// Custom Headers
#include "main.h" // <-- Use our new header
#include "input_parser.h"
#include "command.h"
#include "route.h"
#include "log.h"
#include "jobs.h"

// --- NEW: Global variable to track the foreground process group ---
// `volatile` is crucial because this is modified by a signal handler.
volatile pid_t foreground_pgid = 0;

// --- NEW: Signal handler for Ctrl-C (SIGINT) ---
void sigint_handler(int sig) {
    // If there is a foreground process, send SIGINT to its entire group.
    if (foreground_pgid > 0) {
        kill(-foreground_pgid, SIGINT);
    }
    // The shell itself does nothing, allowing it to reprint the prompt.
    printf("\n"); // Print a newline to look clean
}

// --- NEW: Signal handler for Ctrl-Z (SIGTSTP) ---
void sigtstp_handler(int sig) {
    // If there is a foreground process, send SIGTSTP to its entire group.
    if (foreground_pgid > 0) {
        kill(-foreground_pgid, SIGTSTP);
    }
    // The shell itself does not stop.
}


// Define the global struct here
struct shell_info info;


void init_shell(struct shell_info* info) {
    char* user = getenv("USER");
    if (user != NULL)
    {
    strcpy(info->username, user);
    }
    else
    {
    strcpy(info->username, "roy");
    }

    if (gethostname(info->systemname, sizeof(info->systemname)) != 0)
    {
    strcpy(info->systemname, "iiit");
    }

    if (getcwd(info->cwd, sizeof(info->cwd)) == NULL)
    {
    perror("getcwd() error");
    return;
    }
    strncpy(info->home, info->cwd, sizeof(info->home) - 1);

    info->home[sizeof(info->home) - 1] = '\0'; // Ensure null termination

}


int main() {
    init_shell(&info);
    init_jobs(); // Initialize the job table
    
    //  // --- NEW: Install the signal handlers ---
    // // The shell will now catch SIGINT and SIGTSTP and run our functions.
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);

    
    char command_line[1024];
    while (1) {
        // --- The main loop is now very simple ---
        reap_finished_jobs(); // Check for completed background jobs

        // Taking input
        printf("\033[36m<%s@%s:\033[0m", info.username, info.systemname);
        if (strcmp(info.home, info.cwd) == 0)
        printf("~");
        else
        printf("%s", info.cwd);
        printf("\033[36m> \033[0m");
        fflush(stdout);

        // 2. Get user input (using fgets is safer)
        if (fgets(command_line, sizeof(command_line), stdin) == NULL) {
            printf("logout\n");
            break; // Handle EOF (Ctrl+D)
        }

        // 3. Call the main processing function.
        // For commands typed by the user, we always want to add them to the log (flag = 1).
        process_command_line(command_line, 1);
    }
    return 0;
}

/**
 * @brief This NEW function contains the core logic from your old main loop.
 * It parses and executes a command line. It can be called from main()
 * or from execute_log().
 * @param line The command string to process.
 * @param add_to_history A flag (1 or 0) to control if this command is saved.
 */
void process_command_line(char* line, int add_to_history) {
    // Create a mutable copy of the command line for parsing and logging.
    char command_copy[1024];
    strncpy(command_copy, line, sizeof(command_copy) - 1);
    command_copy[strcspn(command_copy, "\n")] = 0; // Remove trailing newline

    // Handle exit here, as it should terminate the shell immediately.
    if (strcmp(command_copy, "exit") == 0) {
        exit(0);
    }

    // Add to the log ONLY if the flag is set.
    // This prevents commands run via `log execute` from being re-added.
    if (add_to_history) {
        add_to_log(command_copy);
    }

    int sequence_count = 0;
    // The parser creates an array of pipelines, separated by ';'.
    CommandPipeline** pipeline_sequence = parse_command_sequence(command_copy, &sequence_count);

    if (pipeline_sequence) {
        // Loop through and execute each pipeline in the sequence.
        for (int i = 0; i < sequence_count; i++) {
            execute_pipeline(pipeline_sequence[i], command_copy);
        }

        // Clean up all memory used by the parser.
        free_pipeline_sequence(pipeline_sequence, sequence_count);
    } else {
        // Only print error for non-empty commands.
        if (strlen(command_copy) > 0) {
            printf("Invalid Syntax.\n");
        }
    }
}