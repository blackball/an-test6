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

#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <signal.h>

#include "ioutils.h"

unsigned int ENDIAN_DETECTOR = 0x01020304;

static int oldsigbus_valid = 0;
static struct sigaction oldsigbus;
static void sigbus_handler(int sig) {
	fprintf(stderr, "\n\n"
			"SIGBUS (Bus error) signal received.\n"
			"One reason this can happen is that an I/O error is encountered\n"
			"on a file that we are reading with \"mmap\".\n\n"
			"Bailing out now.\n\n");
	fflush(stderr);
	exit(-1);
}

void add_sigbus_mmap_warning() {
	struct sigaction sigbus;
	memset(&sigbus, 0, sizeof(struct sigaction));
	sigbus.sa_handler = sigbus_handler;
	if (sigaction(SIGBUS, &sigbus, &oldsigbus)) {
		fprintf(stderr, "Failed to change SIGBUS handler: %s\n", strerror(errno));
		return;
	}
	oldsigbus_valid = 1;
}

void reset_sigbus_mmap_warning() {
	if (oldsigbus_valid) {
		if (sigaction(SIGBUS, &oldsigbus, NULL)) {
			fprintf(stderr, "Failed to restore SIGBUS handler: %s\n", strerror(errno));
			return;
		}
	}
}

int is_word(char* cmdline, char* keyword, char** cptr) {
	int len = strlen(keyword);
	if (strncmp(cmdline, keyword, len))
		return 0;
	*cptr = cmdline + len;
	return 1;
}

void read_complain(FILE* fin, char* attempted) {
	if (feof(fin)) {
		fprintf(stderr, "Couldn't read %s: end-of-file.\n", attempted);
	} else if (ferror(fin)) {
		fprintf(stderr, "Couldn't read %s: error: %s\n", attempted, strerror(errno));
	} else {
		fprintf(stderr, "Couldn't read %s: %s\n", attempted, strerror(errno));
	}
}

int read_u8(FILE* fin, unsigned char* val) {
    if (fread(val, 1, 1, fin) == 1) {
		return 0;
    } else {
		read_complain(fin, "u8");
		return 1;
    }
}

int read_u16(FILE* fin, unsigned int* val) {
	uint16_t v;
    if (fread(&v, 2, 1, fin) == 1) {
		*val = v;
		return 0;
    } else {
		read_complain(fin, "u8");
		return 1;
    }
}

int read_u32_portable(FILE* fin, unsigned int* val) {
    uint32_t u;
    if (fread(&u, 4, 1, fin) == 1) {
		*val = ntohl(u);
		return 0;
    } else {
		read_complain(fin, "u32");
		return 1;
    }
}

int read_double(FILE* fin, double* val) {
    if (fread(val, sizeof(double), 1, fin) == 1) {
		return 0;
    } else {
		read_complain(fin, "double");
		return 1;
    }
}

int read_u32(FILE* fin, unsigned int* val) {
    uint32_t u;
    if (fread(&u, 4, 1, fin) == 1) {
		*val = (unsigned int)u;
		return 0;
    } else {
		read_complain(fin, "u32 native");
		return 1;
    }
}

int read_u32s_portable(FILE* fin, unsigned int* val, int n) {
    int i;
    uint32_t* u = malloc(sizeof(uint32_t) * n);
    if (!u) {
		fprintf(stderr, "Couldn't real uint32s: couldn't allocate temp array.\n");
		return 1;
    }
    if (fread(u, sizeof(uint32_t), n, fin) == n) {
		for (i=0; i<n; i++) {
			val[i] = ntohl(u[i]);
		}
		free(u);
		return 0;
    } else {
		read_complain(fin, "uint32s");
		free(u);
		return 1;
    }
}

int read_fixed_length_string(FILE* fin, char* s, int length) {
	if (fread(s, 1, length, fin) != length) {
		read_complain(fin, "fixed-length string");
		return 1;
	}
	return 0;
}

char* read_string(FILE* fin) {
	return read_string_terminated(fin, "\0", 1);
}

char* read_string_terminated(FILE* fin, char* terminators, int nterminators) {
	int step = 1024;
	int maxstep = 1024*1024;
	int i = 0;
	int size = 0;
	char* rtn = NULL;
	for (;;) {
		int c = fgetc(fin);
		if (i == size) {
			// expand
			size += step;
			rtn = (char*)realloc(rtn, size);
			if (!rtn) {
				fprintf(stderr, "Couldn't allocate buffer: %i.\n", size+step);
				return rtn;
			}
			if (step < maxstep)
				step *= 2;
		}
		// treat end-of-file like end-of-string.
		if (c == EOF)
			c = '\0';
		rtn[i] = (char)c;
		i++;

		if (memchr(terminators, c, nterminators))
			break;
		/*
		  if (!c)
		  break;
		*/
	}
	if (ferror(fin)) {
		read_complain(fin, "string");
		free(rtn);
		return NULL;
	}
	if (rtn[i-1] != '\0') {
		// add \0 if it isn't already there...
		i++;
	}
	rtn = (char*)realloc(rtn, i);
	// shouldn't happen - we're shrinking.
	if (!rtn) {
		fprintf(stderr, "Couldn't realloc buffer: %i\n", i);
	}
	rtn[i-1] = '\0';
	return rtn;
}

int write_string(FILE* fout, char* s) {
	int len = strlen(s) + 1;
	if (fwrite(s, 1, len, fout) != len) {
		fprintf(stderr, "Couldn't write string: %s\n", strerror(errno));
		return 1;
	}
	return 0;
}

int write_fixed_length_string(FILE* fout, char* s, int length) {
	char* str;
	int res;
	str = calloc(length, 1);
	if (!str) {
		fprintf(stderr, "Couldn't allocate a temp buffer of size %i.\n", length);
		return 1;
	}
	sprintf(str, "%.*s", length, s);
	res = fwrite(str, 1, length, fout);
	free(str);
	if (res != length) {
		fprintf(stderr, "Couldn't write fixed-length string: %s\n", strerror(errno));
		return 1;
	}
	return 0;
}

int write_double(FILE* fout, double val) {
    if (fwrite(&val, sizeof(double), 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write double: %s\n", strerror(errno));
		return 1;
    }
}

int write_float(FILE* fout, float val) {
    if (fwrite(&val, sizeof(float), 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write float: %s\n", strerror(errno));
		return 1;
    }
}

int write_u8(FILE* fout, unsigned char val) {
    if (fwrite(&val, 1, 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u8: %s\n", strerror(errno));
		return 1;
    }
}

int write_u32_portable(FILE* fout, unsigned int val) {
    uint32_t v = htonl((uint32_t)val);
    if (fwrite(&v, 4, 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u32: %s\n", strerror(errno));
		return 1;
    }
}

int write_u32s_portable(FILE* fout, unsigned int* val, int n) {
    int i;
    uint32_t* v = malloc(sizeof(uint32_t) * n);
    if (!v) {
		fprintf(stderr, "Couldn't write u32s: couldn't allocate temp array.\n");
		return 1;
    }
    for (i=0; i<n; i++) {
		v[i] = htonl((uint32_t)val[i]);
    }
    if (fwrite(v, sizeof(uint32_t), n, fout) == n) {
		free(v);
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u32s: %s\n", strerror(errno));
		free(v);
		return 1;
    }
}

int write_u32(FILE* fout, unsigned int val) {
    uint32_t v = (uint32_t)val;
    if (fwrite(&v, 4, 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u32: %s\n", strerror(errno));
		return 1;
    }
}

int write_u16(FILE* fout, unsigned int val) {
    uint16_t v = (uint16_t)val;
    if (fwrite(&v, 2, 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u32: %s\n", strerror(errno));
		return 1;
    }
}

int write_uints(FILE* fout, unsigned int* val, int n) {
    if (fwrite(val, sizeof(unsigned int), n, fout) == n) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write uints: %s\n", strerror(errno));
		return 1;
    }
}

void* buffered_read(bread* br) {
	void* rtn;
	if (!br->buffer) {
		br->buffer = malloc(br->blocksize * br->elementsize);
		br->nbuff = br->off = br->buffind = 0;
	}
	if (br->buffind == br->nbuff) {
		// read a new block!
		uint n = br->blocksize;
		// the new block to read starts after the current block...
		br->off += br->nbuff;
		if (n + br->off > br->ntotal)
			n = br->ntotal - br->off;
		if (!n)
			return NULL;
		memset(br->buffer, 0, br->blocksize * br->elementsize);
		if (br->refill_buffer(br->userdata, br->buffer, br->off, n)) {
			fprintf(stderr, "buffered_read: Error filling buffer.\n");
			return NULL;
		}
		br->nbuff = n;
		br->buffind = 0;
	}
	rtn = (char*)br->buffer + (br->buffind * br->elementsize);
	br->buffind++;
	return rtn;
}

void buffered_read_reset(bread* br) {
	br->nbuff = br->off = br->buffind = 0;
}

void buffered_read_pushback(bread* br) {
	if (!br->buffind) {
		fprintf(stderr, "buffered_read_pushback: Can't push back any further!\n");
		return;
	}
	br->buffind--;
}

void buffered_read_free(bread* br) {
	free(br->buffer);
}
