#ifndef __CLI_DOT_H__
#define __CLI_DOT_H__

void command_line_interpreter(void);

// environment variables
const char *get_environment_variable(const char *name);
void set_environment_variable(const char *name, const char *val); // val=NULL will delete an entry

// fat_fs extensions
const char *f_errmsg(int errno);
void f_perror(int errno);

// execute loaded code (wrapper that ultimately calls machine_execute)
void execute(void *entry_vector);

typedef struct
{
    const char *name;
    const int min_args;
    const int max_args;
    void (* function)(char *argv[], int argc);
    const char *helpme;
} cmd_entry_t;

extern const cmd_entry_t target_cmd_table[];

// cli_fs.c
void do_cd(char *argv[], int argc);
void do_ls(char *argv[], int argc);
void do_rm(char *argv[], int argc);
void do_mkdir(char *argv[], int argc);
void do_mv(char *argv[], int argc);
void do_cp(char *argv[], int argc);
void do_set(char *argv[], int argc);

#endif
