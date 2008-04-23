/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "solvedfile.h"

int solvedfile_getsize(char* fn) {
	FILE* f;
	off_t end;
	f = fopen(fn, "rb");
	if (!f) {
		return -1;
	}
	if (fseek(f, 0, SEEK_END) ||
		((end = ftello(f)) == -1)) {
		fprintf(stderr, "Error: seeking to end of file %s: %s\n",
				fn, strerror(errno));
		fclose(f);
		return -1;
	}
	return (int)end;
}

int solvedfile_get(char* fn, int fieldnum) {
	FILE* f;
	unsigned char val;
	off_t end;

    // 1-index field numbers:
    fieldnum--;

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

// lastfield = 0 for no limit.
static il* solvedfile_getall_val(char* fn, int firstfield, int lastfield, int maxfields, int val) {
	FILE* f;
	off_t end;
	int fields = 0;
	il* list;
	int i;
	unsigned char* map;

	list = il_new(256);

	f = fopen(fn, "rb");
	if (!f) {
		// if file doesn't exist, assume no fields are solved.
		if (val == 0) {
			for (i=firstfield; i<=lastfield; i++) {
				il_append(list, i);
				fields++;
				if (fields == maxfields)
					break;
			}
		}
		return list;
	}

	if (fseek(f, 0, SEEK_END) ||
		((end = ftello(f)) == -1)) {
		fprintf(stderr, "Error: seeking to end of file %s: %s\n",
				fn, strerror(errno));
		fclose(f);
		il_free(list);
		return NULL;
	}
    // 1-index
    firstfield--;
    lastfield--;
	if (end <= firstfield) {
		fclose(f);
		return list;
	}

	map = mmap(NULL, end, PROT_READ, MAP_SHARED, fileno(f), 0);
	fclose(f);
	if (map == MAP_FAILED) {
		fprintf(stderr, "Error: couldn't mmap file %s: %s\n", fn, strerror(errno));
		il_free(list);
		return NULL;
	}

	for (i=firstfield; ((lastfield == -1) || (i<=lastfield)) && (i < end); i++) {
		if (map[i] == val) {
            // 1-index
			il_append(list, i+1);
			if (il_size(list) == maxfields)
				break;
		}
	}

	munmap(map, end);

	if (val == 0) {
		// fields larger than the file size are unsolved.
		for (i=end; i<=lastfield; i++) {
			if (il_size(list) == maxfields)
				break;
            // 1-index
			il_append(list, i+1);
		}
	}
	return list;
}

il* solvedfile_getall(char* fn, int firstfield, int lastfield, int maxfields) {
	return solvedfile_getall_val(fn, firstfield, lastfield, maxfields, 0);
}

il* solvedfile_getall_solved(char* fn, int firstfield, int lastfield, int maxfields) {
	return solvedfile_getall_val(fn, firstfield, lastfield, maxfields, 1);
}

int solvedfile_setsize(char* fn, int sz) {
	int f;
	unsigned char val;
	off_t off;
	f = open(fn, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
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
	if (off < sz) {
		// pad.
		int npad = sz - off;
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
	if (close(f)) {
		fprintf(stderr, "Error closing file %s: %s\n", fn, strerror(errno));
		return -1;
	}
	return 0;
}

int solvedfile_set(char* fn, int fieldnum) {
	int f;
	unsigned char val;
	off_t off;

    // 1-index
    fieldnum--;

	// (file mode 777; umask will modify this, if set).
	f = open(fn, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
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
