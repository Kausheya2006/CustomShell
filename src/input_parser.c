#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
///// CUSTOM HEADERS //////
#include "route.h"
#include "command.h"
/** RULES
 * shell_cmd -> cmd_group ((& | &&) cmd_group)* &?
 * cmd_group -> atomic (\| atomic)*
 * atomic    -> name (name | input | output)*
 * input     -> < name | <name
 * output    -> > name | >name | >> name | >>name
 * name      -> r"[^|&><;]+"
 */

////// UTILITY FUNCTIONS //////
 // skip leading whitespace from current position

/// /////////
void skip_whitespace(char** str)
{
    while (isspace(**str))
    {
        (*str)++;
    }
}

/// From the current position, match a token and advacne the pointer if matched.
int match_token(char** str, char* token) 
{
    skip_whitespace(str);
    if (strncmp(*str, token, strlen(token)) == 0)  // match only till the length of the token
    {
        *str += strlen(token);
        return 1;
    }
    return 0;
}


/////// CFG FUNCTIONS //////
int parse_name(char** str, char** name_buffer) 
{
    skip_whitespace(str);
    char* start = *str;
    // A name must have at least one character and cannot start with a delimiter.
    if (**str == '\0' || strchr("|&><;", **str) != NULL) 
    {
        return 0;
    }
    // Consume characters until a delimiter or whitespace is found.
    while (**str != '\0' && !isspace(**str) && strchr("|&><;", **str) == NULL) 
    {
        (*str)++;
    }
    if (*str > start) // Ensure we consumed at least one character.
    {
        int len = *str - start;
        *name_buffer = malloc(len + 1);
        strncpy(*name_buffer, start, len);
        (*name_buffer)[len] = '\0'; // Null-terminate the string.
        return 1;
    }
    return 0; 
}


int parse_input(char** str, SimpleCommand* cmd) 
{
    char* saved_pos = *str;
    if (match_token(str, "<")) 
    {
        free(cmd->input_file);
        cmd->input_file = NULL;
        if (parse_name(str, &cmd->input_file)) 
        {
            return 1; // Successfully parsed input redirection
        }
    }
    *str = saved_pos; // Backtrack if parsing failed
    return 0;
}


int parse_output(char** str, SimpleCommand* cmd) 
{
    char* saved_pos = *str;

    int is_append = 0;
    if (match_token(str, ">>")) 
    {
        is_append = 1;
    } 
    else if (match_token(str, ">")) 
    {
        is_append = 0;
    } 
    else 
    {
        return 0; // No output token found
    }
    free(cmd->output_file);
    cmd->output_file = NULL;

    if (parse_name(str, &cmd->output_file)) 
    {
        cmd->append_mode = is_append;
        return 1; // Successfully parsed output redirection
    }
    *str = saved_pos; // Backtrack if parsing failed
    return 0;

}


int parse_atomic(char** str, SimpleCommand* cmd) 
{
    // An atomic must start with a name.
    if (!parse_name(str, &cmd->args[cmd->arg_count]))
    {
        return 0; // Failed to parse the command name
    }
    cmd->arg_count++;

    while (1)
    {
        char* loop_start_pos = *str;

        // Try parsing redirections first.
        if (parse_input(str, cmd) || parse_output(str, cmd)) 
        {
            continue; // Successfully parsed a redirection, continue the loop
        }

        // If no redirection was found, try parsing another argument.
        if (parse_name(str, &cmd->args[cmd->arg_count])) 
        {
            cmd->arg_count++;
            continue; // Successfully parsed an argument, continue the loop
        }

        // If nothing was parsed, we are done with this atomic command.
        *str = loop_start_pos; // Backtrack to the start of this iteration
        break;
    }

    cmd->args[cmd->arg_count] = NULL; // Null-terminate the args array
    return 1;
}


int parse_cmd_group(char** str, CommandPipeline* pipeline) 
{
    if (!parse_atomic(str, &pipeline->commands[pipeline->num_commands])) 
    {
        return 0; // Failed to parse the first atomic command
    }
    pipeline->num_commands++;

    while (match_token(str, "|")) 
    {
        if (pipeline->num_commands >= MAX_PIPED_CMDS) 
        {
            return 0; // Exceeded maximum number of piped commands
        }
        if (!parse_atomic(str, &pipeline->commands[pipeline->num_commands])) 
        {
            return 0; // Failed to parse an atomic command after '|'
        }
        pipeline->num_commands++;
    }
    return 1; // Successfully parsed the command group
}

int parse_shell_cmd(char** str, CommandPipeline* pipeline) 
{
    if (!parse_cmd_group(str, pipeline)) 
    {
        return 0; // Failed to parse the first command group
    }

    while (1) 
    {
        char* loop_start_pos = *str;

        if (match_token(str, "&&") || match_token(str, "&")) 
        {
            if (!parse_cmd_group(str, pipeline)) 
            {
                return 0; // Failed to parse a command group after '&&' or '&'
            }
            continue; // Successfully parsed another command group, continue the loop
        }

        // If no operator was found, we are done with the shell command.
        *str = loop_start_pos; // Backtrack to the start of this iteration
        break;
    }
    return 1; // Successfully parsed the entire shell command

}

// Main function to parse the input string into a CommandPipeline structure.
CommandPipeline* parse_commands(char* input) 
{
    CommandPipeline* pipeline = malloc(sizeof(CommandPipeline));
    if (!pipeline) 
    {
        return NULL; // Memory allocation failed
    }
    memset(pipeline, 0, sizeof(CommandPipeline)); // Initialize the structure

    char* str = input;
    skip_whitespace(&str);
    if (!parse_shell_cmd(&str, pipeline)) 
    {
        free(pipeline);
        return NULL; // Parsing failed
    }
    skip_whitespace(&str);
    if (*str != '\0') 
    {
        // Extra unparsed characters at the end
        free(pipeline);
        return NULL;
    }
    return pipeline; // Successfully parsed the input
}

// Free the memory allocated for a CommandPipeline structure.
void free_pipeline(CommandPipeline* pipeline)
{
    if (!pipeline) return;

    for (int i = 0; i < pipeline->num_commands; i++) 
    {
        SimpleCommand* cmd = &pipeline->commands[i];
        for (int j = 0; j < cmd->arg_count; j++) 
        {
            free(cmd->args[j]);
        }
        free(cmd->input_file);
        free(cmd->output_file);
    }
    free(pipeline);
}


CommandPipeline** parse_command_sequence(char* input, int* sequence_count) {
    *sequence_count = 0;
    char* str = input;
    skip_whitespace(&str);
    if (*str == '\0') {
        return NULL; // Nothing to parse.
    }

    // Allocate an array to hold pointers to each pipeline in the sequence.
    CommandPipeline** pipeline_sequence = calloc(MAX_PIPED_CMDS, sizeof(CommandPipeline*));
    if (!pipeline_sequence) {
        perror("shell: calloc");
        return NULL;
    }

    // Loop through the input string, parsing one pipeline at a time.
    while (*str != '\0') {
        if (*sequence_count >= MAX_PIPED_CMDS) {
            fprintf(stderr, "shell: Too many commands in sequence.\n");
            // Clean up and fail
            for (int i=0; i < *sequence_count; i++) free_pipeline(pipeline_sequence[i]);
            free(pipeline_sequence);
            return NULL;
        }

        // Allocate memory for the next pipeline in our sequence.
        pipeline_sequence[*sequence_count] = calloc(1, sizeof(CommandPipeline));
        if (!pipeline_sequence[*sequence_count]) {
            perror("shell: calloc");
            // Clean up and fail
            return NULL; 
        }

        // Use your existing `parse_cmd_group` to parse one full pipeline.
        if (!parse_cmd_group(&str, pipeline_sequence[*sequence_count])) {
            // A syntax error occurred in the pipeline.
            // Clean up and return NULL.
            for (int i=0; i <= *sequence_count; i++) free_pipeline(pipeline_sequence[i]);
            free(pipeline_sequence);
            return NULL;
        }


        pipeline_sequence[*sequence_count]->mode = FOREGROUND;

        if (match_token(&str, "&")) {
            // We found a background operator.
            // Tag the pipeline we JUST parsed as a background job.
            pipeline_sequence[*sequence_count]->mode = BACKGROUND;
        } else if (match_token(&str, ";")) {
            // It's a semicolon. The mode is already FOREGROUND, so we do nothing.
            // This case is just here to consume the ';'.
        } else {
            // No more separators. We are done.
             (*sequence_count)++;
            break; 
        }

        (*sequence_count)++;

        skip_whitespace(&str);
        if (*str == '\0') {
            break; // Trailing separator is allowed.
        }
    }

    skip_whitespace(&str);
    if (*str != '\0') {
        // If there's junk at the end, it's a syntax error.
        // Clean up and fail.
        for (int i=0; i < *sequence_count; i++) free_pipeline(pipeline_sequence[i]);
        free(pipeline_sequence);
        return NULL;
    }

    return pipeline_sequence;
}

/**
 * @brief NEW helper function to free the entire sequence structure.
 */
void free_pipeline_sequence(CommandPipeline** sequence, int count) {
    if (!sequence) return;
    for (int i = 0; i < count; i++) {
        if (sequence[i] != NULL) {
            free_pipeline(sequence[i]);
        }
    }
    free(sequence);
}


// Example usage
/*
int main()
{
    char input[] = "grep foo < input.txt | sort > output.txt &";
    CommandPipeline* pipeline = parse_commands(input);
    if (pipeline)
    {
        // Successfully parsed, do something with the pipeline
        free_pipeline(pipeline);
    }
    else
    {           
        printf("Parsing failed.\n");
    }
    return 0;
}
*/  