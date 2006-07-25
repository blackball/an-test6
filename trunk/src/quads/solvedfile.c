#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "solvedfile.h"

int solvedfile_get(char* fn, int fieldnum) {
	FILE* f;
	unsigned char val;
	off_t end;

	f = fopen(fn, "rb");
	if (!f) {
		// assume it's not solved!
		return 0;
	}
	if (fseek(f, 0, SEEK_END) ||
		((end = ftello(f)) == -1)) {
		fprintf(stderr, "Error: seeking to end of file %s: %s\n",
				fn, strerror(errno));
		fclose(f);
		return -1;
	}
	if (end <= fieldnum) {
		fclose(f);
		return 0;
	}
	if (fseeko(f, (off_t)fieldnum, SEEK_SET) ||
		(fread(&val, 1, 1, f) != 1) ||
		fclose(f)) {
		fprintf(stderr, "Error: seeking, reading, or closing file %s: %s\n",
				fn, strerror(errno));
		fclose(f);
		return -1;
	}
	return val;
}

int solvedfile_set(char* fn, int fieldnum) {
	int f;
	unsigned char val;
	off_t off;
	f = open(fn, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (f == -1) {
		fprintf(stderr, "Error: failed to open file %s for writing: %s\n",
				fn, strerror(errno));
		return -1;
	}
	off = lseek(f, 0, SEEK_END);
	if (off == -1) {
		fprintf(stderr, "Error: failed to lseek() to end of file %s: %s\n", fn, strerror(errno));
		close(f);
		return -1;
	}
	// this gives you the offset one past the end of the file.
	if (off < fieldnum) {
		// pad.
		int npad = fieldnum - off;
		int i;
		val = 0;
		for (i=0; i<npad; i++)
			if (write(f, &val, 1) != 1) {
				fprintf(stderr, "Error: failed to write padding to file %s: %s\n",
						fn, strerror(errno));
				close(f);
				return -1;
			}
	}
	val = 1;
	if ((lseek(f, (off_t)fieldnum, SEEK_SET) == -1) ||
		(write(f, &val, 1) != 1) ||
		close(f)) {
		fprintf(stderr, "Error: seeking, writing, or closing file %s: %s\n",
				fn, strerror(errno));
		close(f);
		return -1;
	}
	return 0;
}

