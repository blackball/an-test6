#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

pthread_t childthread;
pthread_t parentthread;

int quitNow = 0;

uint32_t eventmask;

char* blind = "blind";
char* inputfile = "input";
char* qfile = "queue";
char* qtempfile = "queue.tmp";
pl* q;
pthread_mutex_t qmutex;

void write_queue() {
	int i;
	FILE* fid = fopen(qtempfile, "w");
	if (!fid) {
		fprintf(stderr, "Failed to open file %s: %s\n", qtempfile, strerror(errno));
		return;
	}
	fprintf(stderr, "---- Queue contains: ----\n");
	for (i=0; i<pl_size(q); i++) {
		fprintf(fid, "%s\n", (char*)pl_get(q, i));
		fprintf(stderr, "  %s\n", (char*)pl_get(q, i));
	}
	fprintf(stderr, "-------------------------\n");
	fflush(stderr);
	if (fclose(fid)) {
		fprintf(stderr, "Failed to close file %s: %s\n", qtempfile, strerror(errno));
		return;
	}
	if (rename(qtempfile, qfile)) {
		fprintf(stderr, "Failed to rename %s to %s: %s\n", qtempfile, qfile, strerror(errno));
	}
}

void queue_append(char* str) {
	pthread_mutex_lock(&qmutex);
	fprintf(stderr, "Appending %s\n", str);
	pl_append(q, strdup(str));
	write_queue();
	pthread_mutex_unlock(&qmutex);
}

void queue_pop(char* str) {
	int i;
	pthread_mutex_lock(&qmutex);
	fprintf(stderr, "Popping %s\n", str);
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

//#define PRINT_EVT(x) 	if (evt->mask & (x)) { fprintf(stderr, " " #x); }
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
		fprintf(stderr, "Ignoring file %s.\n", path);
		return;
	}
	queue_append(path);
	fprintf(info->pipeout, "%s\n", path);
	fflush(info->pipeout);
}

void handle_event(childinfo* info, struct inotify_event* evt) {
	int wd;
	char buf[1024];
	char* dir;
	char* path;
	int i;

	// Ignore writes to the queue file.
	if (evt->len && (!strcmp(evt->name, qfile) || !strcmp(evt->name, qtempfile)))
		return;

	// Watch for write to the file "quit".
	if (evt->len && (!strcmp(evt->name, "quit"))) {
		fprintf(info->pipeout, "quit\n");
		fflush(info->pipeout);
		quitNow = 1;
		printf("Child thread quitting.\n");
		pthread_exit(NULL);
	}

	i = il_index_of(info->wds, evt->wd);
	if (i == -1) {
		fprintf(stderr, "wd %i not found.\n", evt->wd);
		return;
	}
	dir = pl_get(info->paths, i);
	//fprintf(stderr, "Dir: %s\n", dir);

	snprintf(buf, sizeof(buf), "%s/%s", dir, evt->name);
	path = buf;
	fprintf(stderr, "Path: %s\n", path);
	fprintf(stderr, "Events:");
	print_events(stderr, evt->mask);
	fprintf(stderr, "\n");

	if ((evt->mask & IN_UNMOUNT) ||
		((evt->mask & IN_DELETE_SELF) && (evt->wd == il_get(info->wds, 0)))) {
		// the base directory went away.
		// NOTE, this doesn't seem to work...
		fprintf(stderr, "Base directory went away.  Quitting.\n");
		fprintf(info->pipeout, "quit\n");
		fflush(info->pipeout);
		pthread_exit(NULL);
	} else if (evt->mask & IN_DELETE_SELF) {
		// a subdirectory we were watching was deleted.
		fprintf(stderr, "Watched directory removed; removing watch.\n");
		inotify_rm_watch(info->inot, evt->wd);
		i = il_index_of(info->wds, evt->wd);
		if (i == -1) {
			fprintf(stderr, "wd not found.\n");
		} else {
			il_remove(info->wds, i);
			pl_remove(info->paths, i);
		}
	} else if ((evt->mask & (IN_CREATE | IN_ISDIR)) == (IN_CREATE | IN_ISDIR)) {
		// a directory was created.
		// add a watch.
		wd = inotify_add_watch(info->inot, path, eventmask);
		if (wd == -1) {
			fprintf(stderr, "Failed to add watch to path %s.\n", path);
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

void child_process(int pipeout) {
	childinfo info;
	char buf[1024];
	char* cwd;
	int wd;
	int i;

	fprintf(stderr, "Child process started.\n");

	info.wds = il_new(16);
	info.paths = pl_new(16);
	info.qfile = qfile;
	info.pipeout = fdopen(pipeout, "w");
	if (!info.pipeout) {
		fprintf(stderr, "Failed to open writing pipe: %s\n", strerror(errno));
		pthread_exit(NULL);
	}

	if ((info.inot = inotify_init()) == -1) {
		fprintf(stderr, "Failed to initialize inotify system: %s\n", strerror(errno));
		pthread_exit(NULL);
	}

	if (!getcwd(buf, sizeof(buf))) {
		fprintf(stderr, "Failed to getcwd(): %s\n", strerror(errno));
		pthread_exit(NULL);
	}
	cwd = buf;
	fprintf(stderr, "Watching: %s\n", cwd);

	eventmask = IN_CLOSE_WRITE | IN_CREATE | IN_MODIFY | IN_MOVED_TO |
		IN_DELETE_SELF;
	//eventmask = IN_ALL_EVENTS;

	if ((wd = inotify_add_watch(info.inot, cwd, eventmask)) == -1) {
		fprintf(stderr, "Failed to add watch to directory %s: %s\n", cwd, strerror(errno));
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
			fprintf(stderr, "End-of-file on inotify file.\n");
			break;
		}
		if (nr == -1) {
			fprintf(stderr, "Error reading from inotify file: %s\n", strerror(errno));
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
			fprintf(stderr, "Failed to remove watch: %s\n", strerror(errno));
			pthread_exit(NULL);
		}
	}

	// FIXME: should free() all contents!
	pl_free(info.paths);
	il_free(info.wds);
	close(info.inot);
	fclose(info.pipeout);

	fprintf(stderr, "Child process finished.\n");
}

void* child_start_routine(void* arg) {
	int* thepipe = (int*)arg;
	child_process(thepipe[1]);
	return NULL;
}

void run(char* path) {
	char* pathcopy;
	char* dir;
	char cmdline[1024];
	int ret;

	pathcopy = strdup(path);
	dir = dirname(pathcopy);
	if (chdir(dir)) {
		fprintf(stderr, "Failed to chdir to %s: %s\n", dir, strerror(errno));
		goto bailout;
	}
	if (snprintf(cmdline, sizeof(cmdline), "%s < %s", blind, path) == -1) {
		fprintf(stderr, "Command line was too long.\n");
		goto bailout;
	}
	free(pathcopy);
	pathcopy = NULL;

	ret = system(cmdline);
	if (ret == -1) {
		fprintf(stderr, "Failed to execute command: \"%s\": %s\n", cmdline, strerror(errno));
		goto bailout;
	}
	if (WIFSIGNALED(ret) &&
		(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT)) {
		fprintf(stderr, "Blind was killed.\n");
		//quitNow = 1;
	} else {
		if (WEXITSTATUS(ret) == 127) {
			fprintf(stderr, "Blind executable not found.  Command-line: \"%s\"\n", cmdline);
		} else {
			fprintf(stderr, "Blind return value: %i\n", WEXITSTATUS(ret));
		}
	}
 bailout:
	free(pathcopy);
}

void parent_process(int pipein) {
	FILE* fin;
	fprintf(stderr, "Parent process started.\n");
	// parent:
	fin = fdopen(pipein, "r");
	if (!fin) {
		fprintf(stderr, "Error fdopening the pipe: %s\n", strerror(errno));
		pthread_exit(NULL);
	}

	for (;;) {
		char str[1024];
		// read a line from the pipe
		if (quitNow)
			break;
		if (!fgets(str, sizeof(str), fin)) {
			if (feof(fin)) {
				fprintf(stderr, "End of file reading from pipe.\n");
				break;
			}
			if (ferror(fin)) {
				fprintf(stderr, "Error reading from pipe: %s\n", strerror(errno));
				break;
			}
			continue;
		}
		// prune the newline from the end of the string
		if (str[strlen(str) - 1] == '\n')
			str[strlen(str) - 1] = '\0';

		printf("Got: %s\n", str);
		if (!strcmp(str, "quit")) {
			printf("Parent thread quitting.\n");
			pthread_exit(NULL);
		}
		printf("Processing %s...\n", str);
		//sleep(3);
		run(str);
		printf("Processed %s\n", str);
		queue_pop(str);
	}

	printf("Parent thread finished.\n");
}

void* parent_start_routine(void* arg) {
	int* thepipe = (int*)arg;
	parent_process(thepipe[0]);
	return NULL;
}

const char* OPTIONS = "hD";

void printHelp(char* progname) {
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int argidx, argchar;
	int thepipe[2];
	int be_daemon = 0;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case 'D':
			be_daemon = 1;
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

	q = pl_new(16);
	pthread_mutex_init(&qmutex, NULL);

	if (pipe(thepipe) == -1) {
		fprintf(stderr, "Error creating pipe: %s\n", strerror(errno));
		exit(-1);
	}

	if (be_daemon) {
		//pthread_detach(childthread);
		//pthread_detach(parentthread);
		//sleep(3);
		printf("Becoming daemon...\n");
		if (daemon(1, 1) == -1) {
			fprintf(stderr, "Failed to set daemon process: %s\n", strerror(errno));
			exit(-1);
		}
	}

	if (pthread_create(&childthread, NULL, child_start_routine, thepipe)) {
		fprintf(stderr, "Failed to create child process: %s\n", strerror(errno));
		exit(-1);
	}

	if (pthread_create(&parentthread, NULL, parent_start_routine, thepipe)) {
		fprintf(stderr, "Failed to create parent process: %s\n", strerror(errno));
		exit(-1);
	}

	//else {
	pthread_join(childthread , NULL);
	pthread_join(parentthread, NULL);
	pthread_mutex_destroy(&qmutex);
	pl_free(q);
	//}
	return 0;
}


