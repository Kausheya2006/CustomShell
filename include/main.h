#ifndef MAIN_H
#define MAIN_H

struct shell_info
{
    char username[40];
    char systemname[40];
    char home[1000];
    char cwd[1000];
};
extern struct shell_info info; // extern indicates its defined in another file

extern volatile pid_t foreground_pgid;

void init_shell(struct shell_info* info);
void process_command_line(char* line, int add_to_history);

#endif