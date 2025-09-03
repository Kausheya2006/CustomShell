#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

// Custom headers
#include "main.h" // For access to the global 'info' struct

char prev_path[1000] = {0};


void execute_hop(char** args) 
{
    
    if (args[1] == NULL) {
        // Before we change directory, store the current one as the previous path.
        strncpy(prev_path, info.cwd, sizeof(prev_path) - 1);
        prev_path[sizeof(prev_path) - 1] = '\0';

        // Attempt to change to the home directory.
        if (chdir(info.home) != 0) {
            // This should rarely fail, but handle it just in case.
            perror("hop: chdir to home failed");
        }
    } else {
        // Process each argument in the sequence provided by the user.
        for (int i = 1; args[i] != NULL; i++) {

            char* arg = args[i];
            char cwd_before_change[1000];

            // Store the current directory before attempting a change.
            if (getcwd(cwd_before_change, sizeof(cwd_before_change)) == NULL) {
                perror("hop: getcwd failed");
                return; // Exit if we can't get the current state.
            }

            int chdir_result = -1; // Default to failure

            if (strcmp(arg, "~") == 0) {
                chdir_result = chdir(info.home);
            } else if (strcmp(arg, "..") == 0) {
                chdir_result = chdir("..");
            } else if (strcmp(arg, ".") == 0) {
                chdir_result = 0; // Success, no operation needed.
            } else if (strcmp(arg, "-") == 0) {
                if (prev_path[0] == '\0') {
                    printf("hop: OLDPWD not set\n");
                    continue; // Skip this argument and move to the next.
                }
                chdir_result = chdir(prev_path);
            } else {
                // This is a path name like "src" or "/etc/".
                chdir_result = chdir(arg);
            }

            // Check if the directory change was successful.
            if (chdir_result == 0) {
                // Success! Update the previous path.
                // The new 'prev_path' is the directory we were just in.
                strncpy(prev_path, cwd_before_change, sizeof(prev_path) - 1);
                prev_path[sizeof(prev_path) - 1] = '\0';
            } else {
                // Failure! Print the error and stop processing further arguments.
                printf("hop: No such directory: %s\n", arg);
                // We don't change info.cwd, as the directory was not changed.
                return;
            }
        }
    }

    // After all arguments have been processed successfully, update the shell's
    // internal CWD tracking variable to reflect the final location.
    if (getcwd(info.cwd, sizeof(info.cwd)) == NULL) {
        perror("hop: failed to get final current directory");
    }
}
