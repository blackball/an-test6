#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <signal.h>

#include <pthread.h>
#include <sys/inotify.h>

#include "bl.h"

/**
   Watches a directory for new input files to appear.
   Maintains a text file showing the queue.
   Runs multiple workers simultaneously.

   Forks a "child" thread to do the actual monitoring and maintenance of the queue file.
   The child writes new input file names on a pipe back to the parent.

   The parent thread reads input filenames one at a time and forks off "worker" threads
   to handle them.
 */

pthread_t childthread;
pthread_t parentthread;

int quitNow = 0;

uint32_t eventmask;

char* blind = "blind < %s";
char* inputfile = "input";
char* qfile = "queue";
char* qtempfile = "queue.tmp";

char* qpath = NULL;
char* qtemppath = NULL;

char* logfile = NULL;
pl* q;
pthread_mutex_t qmutex;
FILE* flog;
char* cwd = NULL;

pthread_mutex_t condmutex;
pthread_cond_t  workercond;
int workers_running = 0;
int nworkers = 1;

void loggit(const char* format, ...) {
	va_list va;
	va_start(va, format);
	vfprintf(flog, format, va);
	fflush(flog);
	fsync(fileno(flog));
	va_end(va);
}

/**
   Writes the current queue to the queue text file.

   Called while holding the "qmutex".
 */
void write_queue() {
	int i;
	FILE* fid;
	fid = fopen(qtemppath, "w");
	if (!fid) {
		loggit("Failed to open file %s: %s\n", qtemppath, strerror(errno));
		return;
	}
	loggit("---- Queue contains: ----\n");
	for (i=0; i<pl_size(q); i++) {
		fprintf(fid, "%s\n", (char*)pl_get(q, i));
		loggit("  %s\n", (char*)pl_get(q, i));
	}
	loggit("-------------------------\n");
	if (fclose(fid)) {
		loggit("Failed to close file %s: %s\n", qtemppath, strerror(errno));
		return;
	}
	if (rename(qtemppath, qpath)) {
		loggit("Failed to rename %s to %s: %s\n", qtemppath, qpath, strerror(errno));
	}
}

/**
   Pushes an item to the queue and updates the queue file.
 */
void queue_append(char* str) {
	pthread_mutex_lock(&qmutex);
	loggit("Queuing %s\n", str);
	pl_append(q, strdup(str));
	write_queue();
	pthread_mutex_unlock(&qmutex);
}

/**
   Pops an item from the queue and updates the queue file.
 */
void queue_pop(char* str) {
	int i;
	pthread_mutex_lock(&qmutex);
	loggit("Popping %s\n", str);
	for (i=0; i<pl_size(q); i++) {
		char* s = pl_get(q, i);
		if (!strcmp(s, str)) {
			pl_remove(q, i);
			free(s);
			break;
		}
	}
	write_queue();
	pthread_mutex_unlock(&qmutex);
}

#define PRINT_EVT(val, tst, fid) 	if ((val) & (tst)) { fprintf(fid, " " #tst); }

void print_events(FILE* fid, uint32_t val) {
	PRINT_EVT(val, IN_ACCESS       , fid);
	PRINT_EVT(val, IN_ATTRIB       , fid);
	PRINT_EVT(val, IN_CLOSE_WRITE  , fid);
	PRINT_EVT(val, IN_CLOSE_NOWRITE, fid);
	PRINT_EVT(val, IN_CREATE       , fid);
	PRINT_EVT(val, IN_DELETE       , fid);
	PRINT_EVT(val, IN_DELETE_SELF  , fid);
	PRINT_EVT(val, IN_MODIFY       , fid);
	PRINT_EVT(val, IN_MOVE_SELF    , fid);
	PRINT_EVT(val, IN_MOVED_FROM   , fid);
	PRINT_EVT(val, IN_MOVED_TO     , fid);
	PRINT_EVT(val, IN_OPEN         , fid);
	PRINT_EVT(val, IN_IGNORED      , fid);
	PRINT_EVT(val, IN_ISDIR        , fid);
	PRINT_EVT(val, IN_Q_OVERFLOW   , fid);
	PRINT_EVT(val, IN_UNMOUNT      , fid);
}

struct childinfo {
	int inot;
	il* wds;
	pl* paths;
	char* qfile;
	FILE* pipeout;
};
typedef struct childinfo childinfo;

/**
   A valid input file was created.  Enqueue it!
 */
void file_created(childinfo* info, struct inotify_event* evt, char* path) {
	char* pathcopy;
	char* base;
	int q_it = 0;
	pathcopy = strdup(path);
	base = basename(pathcopy);
	if (!strcmp(base, inputfile))
		q_it = 1;
	free(pathcopy);
	if (!q_it) {
		//loggit("Ignoring file %s.\n", path);
		return;
	}
	//loggit("Queuing file %s.\n", path);
	queue_append(path);
	fprintf(info->pipeout, "%s\n", path);
	fflush(info->pipeout);
}

/**
   Inotify is delivering an event.  Decide what we need to do anything about it.
 */
void handle_event(childinfo* info, struct inotify_event* evt) {
	int wd;
	char buf[1024];
	char* dir;
	char* path;
	int i;

	// Ignore writes to the queue file and log file.
	if (evt->len && (!strcmp(evt->name, qfile) ||
					 !strcmp(evt->name, qtempfile) ||
					 (logfile && !strcmp(evt->name, logfile))))
		return;

	// Watch for write to the file "quit".
	if (evt->len && (!strcmp(evt->name, "quit"))) {
		fprintf(info->pipeout, "quit\n");
		fflush(info->pipeout);
		quitNow = 1;
		loggit("Child thread quitting.\n");
		pthread_exit(NULL);
	}

	i = il_index_of(info->wds, evt->wd);
	if (i == -1) {
		loggit("wd %i not found.\n", evt->wd);
		return;
	}
	dir = pl_get(info->paths, i);

	snprintf(buf, sizeof(buf), "%s/%s", dir, evt->name);
	path = buf;
	/*
	  loggit("Path: %s\n", path);
	  loggit("Events:");
	  print_events(flog, evt->mask);
	  loggit("\n");
	*/

	if ((evt->mask & IN_UNMOUNT) ||
		((evt->mask & IN_DELETE_SELF) && (evt->wd == il_get(info->wds, 0)))) {
		// the base directory went away.
		// NOTE, this doesn't seem to work...
		loggit("Base directory went away.  Quitting.\n");
		fprintf(info->pipeout, "quit\n");
		fflush(info->pipeout);
		pthread_exit(NULL);
	} else if (evt->mask & IN_DELETE_SELF) {
		// a subdirectory we were watching was deleted.
		loggit("Watched directory removed; removing watch.\n");
		inotify_rm_watch(info->inot, evt->wd);
		i = il_index_of(info->wds, evt->wd);
		if (i == -1) {
			loggit("wd not found.\n");
		} else {
			il_remove(info->wds, i);
			pl_remove(info->paths, i);
		}
	} else if ((evt->mask & (IN_CREATE | IN_ISDIR)) == (IN_CREATE | IN_ISDIR)) {
		// a directory was created.
		// add a watch.
		wd = inotify_add_watch(info->inot, path, eventmask);
		if (wd == -1) {
			loggit("Failed to add watch to path %s.\n", path);
			return;
		}
		il_append(info->wds, wd);
		pl_append(info->paths, strdup(path));
	} else if ((evt->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) &&
			   (!(evt->mask & IN_ISDIR))) {
		// created a file.
		file_created(info, evt, path);
	}
}

/**
   The child thread: listen for Inotify events, maintain the queue and write new
   filenames to the parent.
 */
void child_process(int pipeout) {
	childinfo info;
	char buf[1024];
	int wd;
	int i;

	loggit("Child process started.\n");

	info.wds = il_new(16);
	info.paths = pl_new(16);
	info.qfile = qpath;
	info.pipeout = fdopen(pipeout, "w");
	if (!info.pipeout) {
		loggit("Failed to open writing pipe: %s\n", strerror(errno));
		pthread_exit(NULL);
	}

	if ((info.inot = inotify_init()) == -1) {
		loggit("Failed to initialize inotify system: %s\n", strerror(errno));
		pthread_exit(NULL);
	}

	loggit("Watching: %s\n", cwd);

	eventmask = IN_CLOSE_WRITE | IN_CREATE /*| IN_MODIFY*/ | IN_MOVED_TO | IN_DELETE_SELF;

	if ((wd = inotify_add_watch(info.inot, cwd, eventmask)) == -1) {
		loggit("Failed to add watch to directory %s: %s\n", cwd, strerror(errno));
		pthread_exit(NULL);
	}
	il_append(info.wds, wd);
	pl_append(info.paths, strdup(cwd));

	for (;;) {
		ssize_t nr;
		struct inotify_event* evt;
		ssize_t cursor;
		if (quitNow)
			break;
		nr = read(info.inot, buf, sizeof(buf));
		if (!nr) {
			loggit("End-of-file on inotify file.\n");
			break;
		}
		if (nr == -1) {
			loggit("Error reading from inotify file: %s\n", strerror(errno));
			break;
		}
		cursor = 0;
		while (cursor < nr) {
			evt = (struct inotify_event*)(buf + (int)cursor);
			cursor += sizeof(struct inotify_event) + evt->len;
			handle_event(&info, evt);
		}
	}

	for (i=0; i<il_size(info.wds); i++) {
		wd = il_get(info.wds, i);
		if (inotify_rm_watch(info.inot, wd) == -1) {
			loggit("Failed to remove watch: %s\n", strerror(errno));
			pthread_exit(NULL);
		}
	}

	// FIXME: should free() all contents!
	pl_free(info.paths);
	il_free(info.wds);
	close(info.inot);
	fclose(info.pipeout);

	loggit("Child process finished.\n");
}
void* child_start_routine(void* arg) {
	int* thepipe = (int*)arg;
	child_process(thepipe[1]);
	return NULL;
}

/**
   Run an input file.  Called by the parent (if single-threaded) or by a worker thread.
 */
void run(char* path) {
	char* pathcopy;
	char* dir;
	char cmdline[1024];
	int ret;

	pathcopy = strdup(path);
	dir = dirname(pathcopy);
	if (chdir(dir)) {
		loggit("Failed to chdir to %s: %s\n", dir, strerror(errno));
		goto bailout;
	}
	if (snprintf(cmdline, sizeof(cmdline), blind, path) >= sizeof(cmdline)) {
		loggit("Command line was too long.\n");
		goto bailout;
	}
	free(pathcopy);
	pathcopy = NULL;

	loggit("Runnning: \"%s\"\n", cmdline);
	ret = system(cmdline);
	if (ret == -1) {
		loggit("Failed to execute command: \"%s\": %s\n", cmdline, strerror(errno));
		goto bailout;
	}
	if (WIFSIGNALED(ret) &&
		(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT)) {
		loggit("Blind was killed.\n");
	} else {
		if (WEXITSTATUS(ret) == 127) {
			loggit("Blind executable not found.  Command-line: \"%s\"\n", cmdline);
		} else {
			loggit("Blind return value: %i\n", WEXITSTATUS(ret));
		}
	}
	if (chdir("/")) {
		loggit("Failed to chdir back to /: %s\n", strerror(errno));
		goto bailout;
	}
 bailout:
	free(pathcopy);
}

/**
   A worker thread.  Runs the given task, then signals the parent that it has
   finished.
 */
void* worker_start_routine(void* arg) {
	char* path = (char*)arg;
	char* pathcopy;

	// do it!
	pathcopy = strdup(path);
	run(pathcopy);
	free(pathcopy);

	// Tell the parent I'm finished.
	pthread_mutex_lock(&condmutex);
	workers_running--;
	loggit("Worker finished (now %i running).\n", workers_running);
	pthread_cond_signal(&workercond);
	pthread_mutex_unlock(&condmutex);
	
	return NULL;
}

/**
   Wait for one of the worker threads to finish, then create a new worker thread
   to run the given command.
 */
void start_worker(char* path) {
	pthread_attr_t attribs;
	pthread_mutex_lock(&condmutex);
	while (workers_running == nworkers) {
		// wait for a worker to finish...
		pthread_cond_wait(&workercond, &condmutex);
	}
	workers_running++;
	loggit("Starting a new worker (now %i running).\n", workers_running);
	pthread_mutex_unlock(&condmutex);

	// Create a detached (non-joinable) thread so that its resources
	// are freed when the thread terminates.
	if (pthread_attr_init(&attribs) ||
		pthread_attr_setdetachstate(&attribs, PTHREAD_CREATE_DETACHED)) {
		loggit("Failed to set pthread attributes.\n");
	}

	pthread_t* wthread = calloc(1, sizeof(pthread_t));
	if (!wthread) {
		loggit("Failed to allocate pthread_t for a worker.\n");
		exit(-1);
	}
	if (pthread_create(wthread, &attribs, worker_start_routine, path)) {
		loggit("Failed to pthread_create a worker: %s\n", strerror(errno));
		exit(-1);
	}
}

/**
   The parent thread: read input filenames from the child pipe, and either run
   the command (if single-threaded) or wait for a worker thread to become available
   and launch it.
 */
void parent_process(int pipein) {
	FILE* fin;
	loggit("Parent process started.\n");
	// parent:
	fin = fdopen(pipein, "r");
	if (!fin) {
		loggit("Error fdopening the pipe: %s\n", strerror(errno));
		pthread_exit(NULL);
	}

	for (;;) {
		char str[1024];
		// read a line from the pipe
		if (quitNow)
			break;
		if (!fgets(str, sizeof(str), fin)) {
			if (feof(fin)) {
				loggit("End of file reading from pipe.\n");
				break;
			}
			if (ferror(fin)) {
				loggit("Error reading from pipe: %s\n", strerror(errno));
				break;
			}
			continue;
		}
		// prune the newline from the end of the string
		if (str[strlen(str) - 1] == '\n')
			str[strlen(str) - 1] = '\0';

		loggit("Got: %s\n", str);
		if (!strcmp(str, "quit")) {
			loggit("Parent thread waiting for workers to finish...\n");
			pthread_mutex_lock(&condmutex);
			while (workers_running) {
				pthread_cond_wait(&workercond, &condmutex);
			}
			pthread_mutex_unlock(&condmutex);

			loggit("Parent thread quitting.\n");
			pthread_exit(NULL);
		}
		loggit("Processing %s...\n", str);
		if (nworkers == 1) {
			run(str);
		} else {
			start_worker(str);
		}
		loggit("Processed %s\n", str);
		queue_pop(str);
	}


	loggit("Parent thread finished.\n");
}

void* parent_start_routine(void* arg) {
	int* thepipe = (int*)arg;
	parent_process(thepipe[0]);
	return NULL;
}

const char* OPTIONS = "hDl:c:n:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage:\n\n"
			"%s [options], where options include:\n"
			"      [D]: become a daemon; implies logging to the default\n"
			"           file (\"watcher.log\" in the current directory)\n"
			"           if logging is not specified with -l.\n"
			"      [-l <logfile>]: log to the specified file.\n"
			"      [-c <command>]: run the given command; it should be a printf\n"
			"           pattern; it will be passed the path of the file.\n"
			"           default: \"%s\"\n"
			"      [-n <nthreads>]: number of worker threads to run simultaneously.\n"
			"           default: 1\n"
			"\n", progname, blind);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int argidx, argchar;
	int thepipe[2];
	int be_daemon = 0;
	char buf[1024];

	flog = stderr;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case 'D':
			be_daemon = 1;
			break;
		case 'l':
			logfile = optarg;
			break;
		case 'c':
			blind = optarg;
			break;
		case 'n':
			nworkers = atoi(optarg);
			break;
        case '?':
            fprintf(stderr, "Unknown option `-%c'.\n", optopt);
		case 'h':
			printHelp(args[0]);
			exit(0);
		}
    if (optind < argc) {
        for (argidx = optind; argidx < argc; argidx++)
            fprintf (stderr, "Non-option argument %s\n", args[argidx]);
		printHelp(args[0]);
		exit(-1);
    }

	if (be_daemon && !logfile) {
		logfile = "watcher.log";
	}

	if (logfile) {
		FILE* f = fopen(logfile, "a");
		if (!f) {
			fprintf(stderr, "Failed to open log file %s: %s\n", logfile, strerror(errno));
			exit(-1);
		}
		flog = f;
	}

	q = pl_new(16);
	if (pthread_mutex_init(&qmutex, NULL) ||
		pthread_mutex_init(&condmutex, NULL) ||
		pthread_cond_init(&workercond, NULL)) {
		fprintf(stderr, "Error creating mutexes and conditions: %s\n", strerror(errno));
		exit(-1);
	}

	if (pipe(thepipe) == -1) {
		fprintf(stderr, "Error creating pipe: %s\n", strerror(errno));
		exit(-1);
	}

	if (!getcwd(buf, sizeof(buf))) {
		fprintf(stderr, "Failed to getcwd(): %s\n", strerror(errno));
		exit(-1);
	}
	cwd = strdup(buf);

	if (snprintf(buf, sizeof(buf), "%s/%s", cwd, qtempfile) >= sizeof(buf)) {
		fprintf(stderr, "Path of queue temp file is too long.\n");
		exit(-1);
	}
	qtemppath = strdup(buf);

	if (snprintf(buf, sizeof(buf), "%s/%s", cwd, qfile) >= sizeof(buf)) {
		fprintf(stderr, "Path of queue file is too long.\n");
		exit(-1);
	}
	qpath = strdup(buf);

	if (be_daemon) {
		fprintf(stderr, "Becoming daemon...\n");
		if (daemon(0, 0) == -1) {
			fprintf(stderr, "Failed to set daemon process: %s\n", strerror(errno));
			exit(-1);
		}
	}
	if (logfile) {
		// redirect stdout and stderr to the log.
		if ((dup2(fileno(flog), fileno(stdout)) == -1) ||
			(dup2(fileno(flog), fileno(stderr)) == -1)) {
			fprintf(stderr, "Failed to redirect stdout and stderr to log: %s\n", strerror(errno));
			exit(-1);
		}
	}

	if (pthread_create(&childthread, NULL, child_start_routine, thepipe)) {
		loggit("Failed to create child process: %s\n", strerror(errno));
		exit(-1);
	}

	if (pthread_create(&parentthread, NULL, parent_start_routine, thepipe)) {
		loggit("Failed to create parent process: %s\n", strerror(errno));
		exit(-1);
	}

	pthread_join(childthread , NULL);
	pthread_join(parentthread, NULL);
	pthread_mutex_destroy(&qmutex);
	pthread_mutex_destroy(&condmutex);
	pthread_cond_destroy(&workercond);
	pl_free(q);
	free(cwd);
	free(qpath);
	free(qtemppath);

	if (logfile) {
		if (fclose(flog)) {
			fprintf(stderr, "Failed to close log file: %s\n", strerror(errno));
		}
	}

	return 0;
}


