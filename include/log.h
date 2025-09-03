#ifndef LOG_H
#define LOG_H

// This is the main function to handle the 'log' command and its arguments.
void execute_log(char** args);

// This function is called by the main loop to add a new command to the history.
void add_to_log(const char* command);

#endif
