#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "solvedfile.h"

/*
  static char* fntemplate = NULL;

  int solvedfile_set_filename_template(char* fn) {
  if (fntemplate)
  free(fntemplate);
  fntemplate = strdup(fn);
  return 0;
  }
*/

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
		return -1;
	}
	if (end <= fieldnum) {
		return 0;
	}
	if (fseeko(f, (off_t)fieldnum, SEEK_SET) ||
		(fread(&val, 1, 1, f) != 1) ||
		fclose(f)) {
		fprintf(stderr, "Error: seeking, reading, or closing file %s: %s\n",
				fn, strerror(errno));
		return -1;
	}
	return val;
}

int solvedfile_set(char* fn, int fieldnum) {
	FILE* f;
	unsigned char val;
	off_t off;
	//f = fopen(fn, "ab");
	f = fopen(fn, "wb");
	if (!f) {
		fprintf(stderr, "Error: failed to open file %s for writing: %s\n",
				fn, strerror(errno));
		return -1;
	}
	if (fseeko(f, 0, SEEK_END) ||
		((off = ftello(f)) == -1)) {
		fprintf(stderr, "Error: failed to seek to end of file %s: %s\n",
				fn, strerror(errno));
		return -1;
	}
	printf("End of file is %i.  Fieldnum is %i.\n", (int)off, fieldnum);
	if (off < fieldnum) {
		// pad.
		int npad = fieldnum - off;
		int i;
		val = 0;
		printf("Adding %i pad bytes.\n", npad);
		for (i=0; i<npad; i++)
			if (fwrite(&val, 1, 1, f) != 1) {
				fprintf(stderr, "Error: failed to write padding to file %s: %s\n",
						fn, strerror(errno));
				return -1;
			}
	}
	val = 1;
	if (fseeko(f, (off_t)fieldnum, SEEK_SET) ||
		(fwrite(&val, 1, 1, f) != 1) ||
		fseeko(f, 0, SEEK_END) ||
		fclose(f)) {
		fprintf(stderr, "Error: seeking, writing, or closing file %s: %s\n",
				fn, strerror(errno));
		return -1;
	}
	return 0;
}

