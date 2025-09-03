#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "log.h"
#include "main.h" // For access to process_command_line


#define MAX_HISTORY 15
#define HISTORY_FILENAME ".roy_shell_history"

// Helper function to get the full path to the history file
static void get_history_filepath(char* path_buffer, size_t size) {
    const char* home_dir = getenv("HOME");
    if (home_dir) {
        snprintf(path_buffer, size, "%s/%s", home_dir, HISTORY_FILENAME);
    } else {
        // Fallback if HOME isn't set, though this is rare
        strncpy(path_buffer, HISTORY_FILENAME, size);
    }
}

// Adds a command to the history file, managing the 15-line limit.
void add_to_log(const char* command) {
    // Requirement: Do not store empty commands or 'log' commands.
    if (command == NULL || command[0] == '\0' || strncmp(command, "log", 3) == 0) {
        return;
    }

    char history_path[1024];
    get_history_filepath(history_path, sizeof(history_path));

    char* lines[MAX_HISTORY];
    int line_count = 0;

    FILE* file = fopen(history_path, "r");
    if (file) {
        char buffer[1024];
        while (line_count < MAX_HISTORY && fgets(buffer, sizeof(buffer), file)) {
            buffer[strcspn(buffer, "\n")] = 0; // Remove newline
            if(buffer[0] != '\0') {
                 lines[line_count++] = strdup(buffer);
            }
        }
        fclose(file);
    }

    // Requirement: Do not store a command if it's identical to the previous one.
    if (line_count > 0 && strcmp(lines[line_count - 1], command) == 0) {
        for (int i = 0; i < line_count; i++) free(lines[i]);
        return;
    }

    // Reopen in write mode to overwrite the file
    file = fopen(history_path, "w");
    if (!file) {
        perror("shell: log");
        for (int i = 0; i < line_count; i++) free(lines[i]);
        return;
    }

    int start_index = 0;
    if (line_count == MAX_HISTORY) {
        // If history is full, discard the oldest command by starting from index 1
        start_index = 1;
        free(lines[0]);
    }
    
    // Write the old (and potentially shifted) history back to the file
    for (int i = start_index; i < line_count; i++) {
        fprintf(file, "%s\n", lines[i]);
        free(lines[i]);
    }
    
    // Write the new command
    fprintf(file, "%s\n", command);
    fclose(file);
}

// Handles the logic for `log`, `log purge`, and `log execute <index>`
void execute_log(char** args) {
    char history_path[1024];
    get_history_filepath(history_path, sizeof(history_path));

    if (args[1] == NULL) {
        // --- Behavior: log (no arguments) ---
        FILE* file = fopen(history_path, "r");
        if (file) {
            char buffer[1024];
            while (fgets(buffer, sizeof(buffer), file)) {
                printf("%s", buffer);
            }
            fclose(file);
        }
    } else if (strcmp(args[1], "purge") == 0) {
        // --- Behavior: log purge ---
        FILE* file = fopen(history_path, "w"); // Opening in write mode truncates the file
        if (file) {
            fclose(file);
        }
    } else if (strcmp(args[1], "execute") == 0) {
        // --- Behavior: log execute <index> ---
        if (args[2] == NULL) {
            fprintf(stderr, "log: execute requires an index.\n");
            return;
        }

        int index = atoi(args[2]);
        if (index <= 0) {
            fprintf(stderr, "log: invalid index '%s'.\n", args[2]);
            return;
        }

        char* lines[MAX_HISTORY];
        int line_count = 0;
        FILE* file = fopen(history_path, "r");
        if (!file) {
            fprintf(stderr, "log: history is empty.\n");
            return;
        }

        char buffer[1024];
        while (line_count < MAX_HISTORY && fgets(buffer, sizeof(buffer), file)) {
            buffer[strcspn(buffer, "\n")] = 0;
            lines[line_count++] = strdup(buffer);
        }
        fclose(file);

        if (index > line_count) {
            fprintf(stderr, "log: index %d is out of bounds (history has %d items).\n", index, line_count);
        } else {
            // Index is 1-based from newest to oldest.
            // Newest is at line_count - 1. So we want lines[line_count - index].
            char* command_to_run = lines[line_count - index];
            printf("Executing: %s\n", command_to_run);
            
            process_command_line(command_to_run, 0); // 0 means do not re-add to history
           
        }

        for (int i = 0; i < line_count; i++) free(lines[i]);
    } else {
        fprintf(stderr, "log: invalid argument '%s'. Usage: log [purge | execute <index>]\n", args[1]);
    }
}
