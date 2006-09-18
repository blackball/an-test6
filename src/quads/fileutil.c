#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "fileutil.h"

void get_mmap_size(int start, int size, int* mapstart, int* mapsize, int* pgap) {
	int ps = getpagesize();
	int gap = start % ps;
	// start must be a multiple of pagesize.
	*mapstart = start - gap;
	*mapsize  = size  + gap;
	*pgap = gap;
}

bool file_exists(char* fn) {
	struct stat st;
	return (stat(fn, &st) == 0);
}

char *mk_filename(const char *basename, const char *extension)
{
	char *fname;
	fname = malloc(strlen(basename) + strlen(extension) + 1);
	sprintf(fname, "%s%s", basename, extension);
	return fname;
}

void fopenout(char* fn, FILE** pfid) {
	FILE* fid = fopen(fn, "wb");
	if (!fid) {
		fprintf(stderr, "Error opening file %s: %s\n", fn, strerror(errno));
		exit(-1);
	}
	*pfid = fid;
}

