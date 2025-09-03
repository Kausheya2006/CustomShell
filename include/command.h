// command.h
#ifndef COMMAND_H
#define COMMAND_H

#define MAX_ARGS 64       // Max arguments for a single command
#define MAX_PIPED_CMDS 16 // Max commands in a single pipeline

// Represents one command in a pipeline (e.g., "grep foo")
typedef struct {
    char* args[MAX_ARGS];  // Argument list like {"grep", "foo", NULL}
    char* input_file;      // Redirect stdin from this file
    char* output_file;     // Redirect stdout to this file
    int   append_mode;       // Flag for append mode (>>)
    int   arg_count;       // Number of arguments
} SimpleCommand;

typedef enum {
    FOREGROUND,
    BACKGROUND
} JobMode;

// Represents a full pipeline (e.g., "cat file.txt | grep foo")
typedef struct {
    SimpleCommand commands[MAX_PIPED_CMDS];
    int num_commands;
    JobMode mode;
} CommandPipeline;

// Function prototype for the new parser
CommandPipeline* parse_commands(char* input);
// Function to free the memory used by the pipeline
void free_pipeline(CommandPipeline* pipeline);


#endif