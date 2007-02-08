#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include <linux/inotify.h>

#include "bl.h"

void child_process() {
	int inot;
	pl* q;
	char buf[1024];
	char* cwd;
	uint32_t eventmask;
	int wd;

	q = pl_new(16);

	fflush(NULL);
	fprintf(stderr, "Child process started.\n");
	fflush(NULL);

	if ((inot = inotify_init()) == -1) {
		fprintf(stderr, "Failed to initialize inotify system: %s\n", strerror(errno));
		exit(-1);
	}

	if (!getcwd(buf, sizeof(buf))) {
		fprintf(stderr, "Failed to getcwd(): %s\n", strerror(errno));
		exit(-1);
	}
	//cwd = strdup(buf);
	cwd = buf;

	eventmask = IN_CLOSE_WRITE | IN_CREATE | IN_MODIFY | IN_MOVED_TO;

	if ((wd = inotify_add_watch(inot, cwd, eventmask)) == -1) {
		fprintf(stderr, "Failed to add watch to directory %s: %s\n", cwd, strerror(errno));
		exit(-1);
	}

	fprintf(stderr, "Writing...\n");
	printf("Hello\n");
	fflush(stdout);
	fprintf(stderr, "Wrote.\n");
	sleep(5);
	fprintf(stderr, "Writing...\n");
	printf("World\n");
	fflush(stdout);
	fprintf(stderr, "Wrote.\n");

	if (inotify_rm_watch(inot, wd) == -1) {
		fprintf(stdrer, "Failed to remove watch: %s\n", strerror(errno));
		exit(-1);
	}

	pl_free(q);
	//free(cwd);
	close(inot);
}

int main(int argc, char** args) {
	int thepipe[2];
	pid_t pid;
	FILE* fin;

	if (pipe(thepipe) == -1) {
		fprintf(stderr, "Error creating pipe: %s\n", strerror(errno));
		exit(-1);
	}

	if ((pid = fork()) == -1) {
		fprintf(stderr, "Error forking: %s\n", strerror(errno));
		exit(-1);
	} else if (pid == 0) {
		// child
		if ((dup2(thepipe[1], fileno(stdout)) == -1) ||
			(close(thepipe[0]) == -1) ||
			(close(thepipe[1]) == -1)) {
			fprintf(stderr, "Error in dup2/close: %s\n", strerror(errno));
			exit(-1);
		}
		child_process();
		exit(0);
	}

	// parent:
	if (close(thepipe[1]) == -1) {
		fprintf(stderr, "Error closing pipe: %s\n", strerror(errno));
		exit(-1);
	}

	fin = fdopen(thepipe[0], "r");
	if (!fin) {
		fprintf(stderr, "Error fdopening the pipe: %s\n", strerror(errno));
		exit(-1);
	}

	for (;;) {
		char str[1024];
		// read a line from the pipe
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
		printf("Processing %s ...\n", str);
		sleep(10);
		printf("Done processing %s.\n", str);

	}

	return 0;
}


