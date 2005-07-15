/*
   File:         am_proc.h
   Author(s):    jkomarek
   Created:      August 2003
   Description:  
   Copyright (C) Carnegie Mellon University
*/

#ifndef EXEC_UTILS_H
#define EXEC_UTILS_H

#include "standard.h"
#include "ambs.h"

/*file descriptor definition is different on Unix and windows*/
#if defined(PC_PLATFORM)
#define FILE_DESC HANDLE
#define INVALID_FD 0
#include <windows.h>
#include <process.h>
#elif defined(UNIX_PLATFORM)
#define FILE_DESC int
#define INVALID_FD -1
#endif

FILE_DESC am_stdin_fd(void);
FILE_DESC am_stdout_fd(void);
FILE_DESC am_stderr_fd(void);

long am_execute(const char *path, char *const argv[]);
/*int am_wait(long pid, int *status);*/
int am_kill(long pid);
bool am_child_alive(long pid);
size_t read_from_fd(FILE_DESC fd, char *buff,  size_t count, bool do_block);
size_t write_to_fd(FILE_DESC fd, const char *buff,  size_t count);
void am_close_fd(FILE_DESC fd);

char *read_all_from_fd(FILE_DESC fd, bool do_block);
void write_all_to_fd(FILE_DESC fd, const char *msg);

/*for sending to microsoft CreateProcess calls*/
char *mk_cmdline_from_args(const char *path, char *const argv[]);


#endif /*#ifndef EXEC_UTILS_H*/
