#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "ping.h"

/**
 * @brief Implements the 'ping' built-in command.
 * Sends a specified signal to a process with a given PID.
 * @param args The null-terminated array of arguments from the parser.
 * args[0] is "ping", args[1] is the PID, args[2] is the signal number.
 */
void execute_ping(char** args) {
    // --- 1. Argument Validation ---
    if (args[1] == NULL || args[2] == NULL) {
        fprintf(stderr, "ping: Invalid syntax. Usage: ping <pid> <signal_number>\n");
        return;
    }

    // Convert string arguments to integers
    pid_t pid = atoi(args[1]);
    int signal_number = atoi(args[2]);

    if (pid <= 0) {
        fprintf(stderr, "ping: Invalid PID '%s'\n", args[1]);
        return;
    }

    // --- 2. Calculate the Actual Signal ---
    // Per the requirement, take the signal number modulo 32.
    int actual_signal = signal_number % 32;

    // --- 3. Send the Signal using kill() ---
    // The kill() system call is used to send any signal to any process group or process.
    // It returns 0 on success and -1 on error.
    if (kill(pid, actual_signal) == 0) {
        // Success case
        printf("Sent signal %d to process with pid %d\n", signal_number, pid);
    } else {
        // Error case. We check the `errno` variable to see why it failed.
        if (errno == ESRCH) {
            // ESRCH means "No such process". This is the specific required message.
            printf("No such process found\n");
        } else {
            // For other errors (like permission denied), print a generic message.
            perror("ping: kill");
        }
    }
}