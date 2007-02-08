#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include <pthread.h>
#include <sys/inotify.h>

#include "bl.h"

pthread_t childthread;
uint32_t eventmask;
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

#define PRINT_EVT(x) 	if (evt->mask & (x)) { fprintf(stderr, " " #x); }

struct childinfo {
	int inot;
	il* wds;
	pl* paths;
	char* qfile;
	FILE* pipeout;
};
typedef struct childinfo childinfo;

void file_created(childinfo* info, struct inotify_event* evt, char* path) {
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

	fprintf(stderr, "Events:");
	PRINT_EVT(IN_ACCESS);
	PRINT_EVT(IN_ATTRIB);
	PRINT_EVT(IN_CLOSE_WRITE);
	PRINT_EVT(IN_CLOSE_NOWRITE);
	PRINT_EVT(IN_CREATE);
	PRINT_EVT(IN_DELETE);
	PRINT_EVT(IN_DELETE_SELF);
	PRINT_EVT(IN_MODIFY);
	PRINT_EVT(IN_MOVE_SELF);
	PRINT_EVT(IN_MOVED_FROM);
	PRINT_EVT(IN_MOVED_TO);
	PRINT_EVT(IN_OPEN);
	PRINT_EVT(IN_IGNORED);
	PRINT_EVT(IN_ISDIR);
	PRINT_EVT(IN_Q_OVERFLOW);
	PRINT_EVT(IN_UNMOUNT);
	fprintf(stderr, "\n");

	i = il_index_of(info->wds, evt->wd);
	if (i == -1) {
		fprintf(stderr, "wd %i not found.\n", evt->wd);
		return;
	}
	dir = pl_get(info->paths, i);
	fprintf(stderr, "Dir: %s\n", dir);

	snprintf(buf, sizeof(buf), "%s/%s", dir, evt->name);
	path = buf;
	fprintf(stderr, "Path: %s\n", path);

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
}

void* start_routine(void* arg) {
	int* thepipe = (int*)arg;
	child_process(thepipe[1]);
	return NULL;
}

int main(int argc, char** args) {
	int thepipe[2];
	FILE* fin;

	q = pl_new(16);
	pthread_mutex_init(&qmutex, NULL);

	if (pipe(thepipe) == -1) {
		fprintf(stderr, "Error creating pipe: %s\n", strerror(errno));
		exit(-1);
	}

	if (pthread_create(&childthread, NULL, start_routine, thepipe)) {
		fprintf(stderr, "Failed to create child process: %s\n", strerror(errno));
		exit(-1);
	}

	// parent:
	fin = fdopen(thepipe[0], "r");
	if (!fin) {
		fprintf(stderr, "Error fdopening the pipe: %s\n", strerror(errno));
		exit(-1);
	}

	for (;;) {
		char str[1024];
		// read a line from the pipe
		//fprintf(stderr, "Main thread: waiting for input...\n");
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
		printf("Processing %s...\n", str);
		sleep(3);
		printf("Processed %s\n", str);
		queue_pop(str);
	}

	printf("Main thread finished.\n");

	pthread_mutex_destroy(&qmutex);
	pl_free(q);

	//pthread_join(&childthread);

	return 0;
}


