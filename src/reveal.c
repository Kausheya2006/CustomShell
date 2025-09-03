#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

// Custom headers
#include "main.h" // For access to the global 'info' struct
#include "hop.h"  // For access to prev_path
#include "reveal.h"

extern char prev_path[1000];

static int string_comparator(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}


char** list_and_sort_files(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) {
        // Error will be printed by the caller
        return NULL;
    }

    struct dirent* entry;
    char** files = NULL;
    int count = 0;
    int capacity = 20;

    files = malloc(capacity * sizeof(char*));
    if (!files) {
        perror("reveal: malloc");
        closedir(dir);
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (count >= capacity - 1) { // -1 for the NULL terminator
            capacity *= 2;
            char** tmp = realloc(files, capacity * sizeof(char*));
            if (!tmp) {
                perror("reveal: realloc");
                for (int i = 0; i < count; i++) free(files[i]);
                free(files);
                closedir(dir);
                return NULL;
            }
            files = tmp;
        }
        files[count] = strdup(entry->d_name);
        count++;
    }
    closedir(dir);

    files[count] = NULL; // Null-terminate the array

    // Sort the file list lexicographically
    qsort(files, count, sizeof(char*), string_comparator);

    return files;
}


void print_files(char** files, int show_hidden, int line_by_line) {
    if (files == NULL) return;

    for (int i = 0; files[i] != NULL; i++) {
        if (!show_hidden && files[i][0] == '.') {
            continue; // Skip hidden files if -a is not set
        }

        if (line_by_line) {
            printf("%s\n", files[i]);
        } else {
            printf("%s  ", files[i]);
        }
        free(files[i]); // Free each string
    }

    if (!line_by_line) {
        printf("\n");
    }

    free(files); // Free the array itself
}

/**
 * @brief Executes the 'reveal' command.
 * @param args Null-terminated array of strings from the parser.
 */
void execute_reveal(char** args) {
    int show_hidden = 0;
    int line_by_line = 0;
    char* path_arg = NULL;

    // 1. Parse arguments for flags and the optional path.
    for (int i = 1; args[i] != NULL; i++) {
        if (args[i][0] == '-') {
            // This is a flag argument, e.g., "-al"
            for (int j = 1; args[i][j] != '\0'; j++) {
                if (args[i][j] == 'a') {
                    show_hidden = 1;
                } else if (args[i][j] == 'l') {
                    line_by_line = 1;
                }
            }
        } else {
            // This is a path argument.
            if (path_arg != NULL) {
                printf("reveal: Invalid Syntax!\n");
                return; // More than one path argument provided.
            }
            path_arg = args[i];
        }
    }

    // 2. Determine the target directory path.
    char target_path[1024];
    if (path_arg == NULL) {
        // No path provided, use the current working directory.
        strncpy(target_path, info.cwd, sizeof(target_path) -1);
    } else if (strcmp(path_arg, "~") == 0) {
        strncpy(target_path, info.home, sizeof(target_path) -1);
    } else if (strcmp(path_arg, "-") == 0) {
        if (prev_path[0] == '\0') {
            printf("No such directory!\n"); // Per requirement for 'reveal -'
            return;
        }
        strncpy(target_path, prev_path, sizeof(target_path) - 1);
    } else {
        // For ".", "..", or a named path, just use the argument directly.
        // opendir will handle resolving these relative to the CWD.
        strncpy(target_path, path_arg, sizeof(target_path) - 1);
    }
    target_path[sizeof(target_path) - 1] = '\0';

    // 3. List, sort, and print the files.
    char** files = list_and_sort_files(target_path);
    if (files == NULL) {
        printf("No such directory!\n");
        return;
    }

    // Default behavior if no flags are set.
    if (!show_hidden && !line_by_line) {
        // Print in multi-column format, without hidden files.
        print_files(files, 0, 0);
    } else {
        // Print according to the flags set.
        print_files(files, show_hidden, line_by_line);
    }
}
