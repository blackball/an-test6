/*
   File:         am_proc.c
   Author(s):    jkomarek
   Created:      August 2003
   Description:  cross platform code to determine the install dir of a process
   Copyright (C) Carnegie Mellon University
*/

/*make sure this is the first include so PC_PLATFORM, etc are defined*/
#include "standard.h"
#include "am_string.h"

/*--- Begin Platform dependent includes ---*/
#if defined(UNIX_PLATFORM) /*UNIX includes*/
#include <unistd.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>

#elif defined(PC_PLATFORM) /*MSW includes*/
#include <windows.h>

#endif 
/*--- End Platform dependent includes ---*/

#include "ambs.h"
#include "am_string_array.h"
#include "am_proc.h"

/*---------------- definitions -----------------------*/


/********************* GENERAL CODE ************************/

char *mk_cmdline_from_args(const char *path, char *const argv[])
{
    char *cmd_line;
    char *str;
    int i;
    string_array *all_args = mk_string_array(0);

    str = mk_printf("\"%s\"", path);
    string_array_add(all_args, str);
    free_string(str);
    for (i = 1; argv[i] != NULL; i++)
    {
        str = mk_printf("\"%s\"", argv[i]);    
        string_array_add(all_args, str);
        free_string(str);
    }
    cmd_line = mk_string_from_string_array_with_separator(all_args,' ');
    free_string_array(all_args);
    return cmd_line;
}

/* Read everything available on the specified file descriptor.
   If 'do_block' is TRUE, then won't return until it has read 
   something.  Return the string read.
*/
char *read_all_from_fd(FILE_DESC fd, bool do_block)
{
    char buff[1024];
    char *out_str = NULL;
    char *tmp = NULL;
    bool block = do_block;

    if (fd == INVALID_FD) return NULL;
    
    while (read_from_fd(fd, buff, 1024, block) > 0)
    {
        block = FALSE; /*block for first read only*/
	if (out_str)
	{
	    tmp = mk_printf("%s%s", out_str, buff);
	    free_string(out_str);
	}
	else
	{
	    tmp = mk_copy_string(buff);
	}
	out_str = tmp;

	if ((int)strlen(out_str) + 1024 >= get_printf_buffer_size())
	{
	    /*don't read any more - mk_printf can't handle it*/
	    break;
	}
    }
    return out_str;
}

/* don't return until the entire message has been written to
   the file descriptor (sometimes a single call to write_to_fd
   isn't enough to write the whole msg)
*/
void write_all_to_fd(FILE_DESC fd, const char *msg)
{
    const char *s;
    int nbytes;
    bool more;
    int total_written = 0;
    int num_written = 0;

    if (fd != INVALID_FD && msg)
    {
	nbytes = strlen(msg);
	more = TRUE;
	s = msg;
	while (more)
	{
	    num_written = write_to_fd(fd, s, nbytes); 
	    if (num_written > 0)
	    {
		total_written += num_written;
		nbytes -= num_written;
		s += num_written;
	    }
	    more = (num_written > 0 && nbytes > 0);
	}
    }
}



/********************* UNIX CODE **************************/
#ifdef UNIX_PLATFORM

FILE_DESC am_stdin_fd(void)
{
    return STDIN_FILENO;
}
FILE_DESC am_stdout_fd(void)
{
    return STDOUT_FILENO;
}
FILE_DESC am_stderr_fd(void)
{
    return STDERR_FILENO;
}

/*
  Launch the child process specified by 'path'.
  The first argument in argv, by convention, should point to the file name 
  associated with the file being executed.  The list  of  arguments  must  be
  terminated by a NULL pointer.
*/
long am_execute(const char *path, char *const argv[])
{
    pid_t pid;

    pid = fork();
    if (pid == 0)
    {
        /*child process*/
        execvp(path, argv);
	exit (0); /*should only get here on error*/
    }
 
    return pid;
}

void am_close_fd(FILE_DESC fd)
{
    close(fd);
}


bool am_child_alive(long pid)
{
    int status;
    pid_t ret = waitpid(pid, &status, WNOHANG);
    if (ret == pid || ret == -1)
    {
        return FALSE;
    }
    return TRUE;
}

int am_kill(long pid)
{
    return kill(pid, SIGTERM);
}

bool read_wont_block(FILE_DESC fd)
{
    fd_set readfds;
    struct timeval tv = {0,0};
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    select(fd + 1, &readfds, 0, 0, &tv);
    if (FD_ISSET(fd, &readfds))
    {
	return TRUE;
    }
    return FALSE;
}

size_t read_from_fd(FILE_DESC fd, char *buff,  size_t count, bool do_block)
{
    size_t num_read = 0;

    if (do_block || read_wont_block(fd))
    {
	num_read = read(fd, buff, count-1);
	buff[num_read] = '\0';
    }

    return num_read;
}

size_t write_to_fd(FILE_DESC fd, const char *buff, size_t count)
{
    size_t num_written = 0;
    
    fd_set writefds;
    struct timeval tv = {0,0};
    FD_ZERO(&writefds);
    FD_SET(fd, &writefds);

    select(fd + 1, 0, &writefds, 0, &tv);
    if (FD_ISSET(fd, &writefds))
    {
	num_written = write(fd, buff, count);
    }
    return num_written;
}

#endif

/******************** WIN 32 CODE *************************/
#ifdef PC_PLATFORM

FILE_DESC am_stdin_fd(void)
{
    return GetStdHandle(STD_INPUT_HANDLE);
}
FILE_DESC am_stdout_fd(void)
{
    return GetStdHandle(STD_OUTPUT_HANDLE);
}
FILE_DESC am_stderr_fd(void)
{
    return GetStdHandle(STD_ERROR_HANDLE);
}

long am_execute(const char *path, char *const argv[])
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    char *cmd_line = mk_cmdline_from_args(path, argv);

    /* Start the child process. */
    GetStartupInfo(&si);
    pi.dwProcessId = -1;
    if( !CreateProcess(NULL,  /* app to launch */
        cmd_line,             /* Command line. */
        NULL,                 /* Process handle not inheritable. */
        NULL,                 /* Thread handle not inheritable. */
        FALSE,                /* Set handle inheritance to FALSE.  */
        0,                    /* No creation flags. */
        NULL,                 /* Use parent's environment block. */
        NULL,                 /* Use parent's starting directory. */
        &si,                  /* Pointer to STARTUPINFO structure.*/
        &pi )                 /* Pointer to PROCESS_INFORMATION structure.*/
    ) 
    {
        DWORD err = GetLastError();
    }
    free_string(cmd_line);

    /* Close process handle */
    CloseHandle( pi.hProcess );

    return pi.dwProcessId;
}

void am_close_fd(FILE_DESC fd)
{
    CloseHandle(fd);
}


bool am_child_alive(long pid)
{
    bool ret = TRUE;
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);
    DWORD exit_code = 0;

    if (hProcess == NULL) /*on some os's the terminated process handle*/
    {                     /*can't be opened*/
	ret = FALSE;
    }
    else
    {
	GetExitCodeProcess(hProcess, &exit_code);
	CloseHandle(hProcess);
	
	if (exit_code == STILL_ACTIVE)
	{
	    ret = TRUE;
	}
	else
	{
	    ret = FALSE;
	}
    }
    return ret;
}

int am_kill(long pid)
{
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);
    if (hProcess != NULL) 
    {
        TerminateProcess(hProcess, 0);
    }
    CloseHandle(hProcess);
    return 0;
}

size_t write_to_fd(FILE_DESC fd, const char *buff, size_t count)
{
    DWORD num_written = 0;
    DWORD err;
    WriteFile(fd, buff, count, &num_written, 0);
    err=GetLastError();
    return num_written;
}

size_t read_from_fd(FILE_DESC fd, char *buff,  size_t count, bool do_block)
{
    size_t num_read = 0;
    DWORD num_pending = 0;

    PeekNamedPipe(fd, 0, 0, 0, &num_pending, 0);
    if (do_block || num_pending > 0)
    {
        ReadFile(fd, buff, count-1, &num_read, 0);
        buff[num_read] = '\0';
    }
    return num_read;
}

#endif
/**********************************************************/

