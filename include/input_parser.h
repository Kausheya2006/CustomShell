#ifndef INPUT_PARSER_H
#define INPUT_PARSER_H

#include "command.h" // For CommandPipeline

CommandPipeline** parse_command_sequence(char* input, int* sequence_count);
void free_pipeline_sequence(CommandPipeline** sequence, int count);
int isValidShellCommand(char* input);
int match_token(char** str, const char* token);
void skip_whitespace(char** str);
int parse_name(char** str);
#endif