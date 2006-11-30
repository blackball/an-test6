/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <libgen.h>

#include "fileutil.h"
#include "xylist.h"
#include "bl.h"

#define OPTIONS "chdx:y:t:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s [options] <input-file> <output-file>\n"
		   "    [-c]: read simple 'x y\\n' format; the input is a single field.\n"
		   "    [-d]: write double format (float format is default).\n"
		   "    [-x <name-of-x-column-to-write>] (default: X)\n"
		   "    [-y <name-of-y-column-to-write>] (default: Y)\n"
		   "    [-t <Astrometry.net filetype>] (default: %s)\n\n",
		   progname, AN_FILETYPE_XYLS);
}

typedef pl xyarray;
#define mk_xyarray(n)         pl_new(n)
#define free_xyarray(l)       pl_free(l)
#define xya_ref(l, i)    (xy*)pl_get((l), (i))
#define xya_set(l, i, v)      pl_set((l), (i), (v))
#define xya_size(l)           pl_size(l)

xyarray *readxy(char* fn);
xyarray *readxysimple(char* fn);

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	char* infn = NULL;
	char* outfn = NULL;
    int c;
    int simple = 0;
	xyarray* xya;
	xylist* ls;
	int i, N;
	int doubleformat = 0;
	char* xname = NULL;
	char* yname = NULL;
	char* antype = NULL;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'd':
			doubleformat = 1;
			break;
		case 'x':
			xname = optarg;
			break;
		case 'c':
			simple = 1;
			break;
		case 'y':
			yname = optarg;
			break;
		case 't':
			antype = optarg;
			break;
		}
    }

	if ((optind+2) < argc) {
		print_help(args[0]);
		exit(-1);
	}

	infn = args[optind];
	outfn = args[optind + 1];

	if (!infn || !outfn) {
		print_help(args[0]);
		exit(-1);
	}

	printf("input %s, output %s.\n", infn, outfn);

	printf("reading input...\n");

	if (simple) {
		xya = readxysimple(infn);
	} else {
		xya = readxy(infn);
	}
	if (!xya)
		exit(-1);

	ls = xylist_open_for_writing(outfn);
	if (!ls)
		exit(-1);
	if (doubleformat)
		ls->xtype = ls->ytype = TFITS_BIN_TYPE_D;
	qfits_header_add(ls->header, "WRITER", basename(args[0]), "This file was written by the program...", NULL);
	if (xname)
		ls->xname = xname;
	if (yname)
		ls->yname = yname;
	if (xylist_write_header(ls))
		exit(-1);

	if (antype) {
		ls->antype = antype;
	}

	N = xya_size(xya);
	printf("writing %i fields...\n", N);
	for (i=0; i<N; i++) {
		int j, M;
		xy* list = xya_ref(xya, i);
		if (!list) {
			fprintf(stderr, "list %i is null.", i);
			continue;
		}
		xylist_write_new_field(ls);
		M = xy_size(list);
		for (j=0; j<M; j++) {
			double vals[2];
			vals[0] = xy_refx(list, j);
			vals[1] = xy_refy(list, j);
			xylist_write_entries(ls, vals, 1);
		}
		xylist_fix_field(ls);
	}
	xylist_fix_header(ls);
	xylist_close(ls);

	return 0;
}

typedef unsigned short int magicval;
#define MAGIC_VAL 0xFF00
#define ASCII_VAL 0x754E

xyarray *readxy(char* fn) {
	uint ii, jj, numxy, numfields;
	magicval magic;
	xyarray *thepix = NULL;
	int tmpchar;
	FILE* fid;

	fid = fopen(fn, "r");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s to read xylist.\n", fn);
		return NULL;
	}
	if (fread(&magic, sizeof(magic), 1, fid) != 1) {
		fprintf(stderr, "ERROR (readxy) -- bad magic value in field file.\n");
		return NULL;
	}
	if (magic != ASCII_VAL) {
		fprintf(stderr, "Error - bad magic value.\n");
		return NULL;
	}
	if (fscanf(fid, "mFields=%u\n", &numfields) != 1) {
		fprintf(stderr, "ERROR (readxy) -- bad first line in field file.\n");
		return NULL;
	}
	thepix = mk_xyarray(numfields);
	for (ii = 0;ii < numfields;ii++) {
		tmpchar = fgetc(fid);
		while (tmpchar == COMMENT_CHAR) {
			fscanf(fid, "%*[^\n]");
			fgetc(fid);
			tmpchar = fgetc(fid);
		}
		ungetc(tmpchar, fid);
		fscanf(fid, "%u", &numxy); // CHECK THE RETURN VALUE MORON!

		xya_set(thepix, ii, mk_xy(numxy) );

		if (xya_ref(thepix, ii) == NULL) {
			fprintf(stderr, "ERROR (readxy) - out of memory at field %u\n", ii);
			free_xyarray(thepix);
			return NULL;
		}
		for (jj = 0;jj < numxy;jj++) {
			double tmp1, tmp2;
			fscanf(fid, ",%lf,%lf", &tmp1, &tmp2);
			xy_setx(xya_ref(thepix, ii), jj, tmp1);
			xy_sety(xya_ref(thepix, ii), jj, tmp2);
		}
		fscanf(fid, "\n");
	}
	return thepix;
}

// Read a simple xy file; column 1 is x column 2 is y; space seperated.
xyarray *readxysimple(char* fn) {
	uint numxy=0;
	xyarray *thepix = NULL;
	FILE* fid;

	fid = fopen(fn, "r");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s to read xylist.\n", fn);
		return NULL;
	}

	// find how many stars there are
	while (1) {
		double x,y;
		int n = fscanf(fid, "%lf %lf\n", &x, &y);
		if (n != 2)
			break;
		numxy++;
	}
	rewind(fid);

	thepix = mk_xyarray(1);

	xya_set(thepix, 0, mk_xy(numxy) );

	if (xya_ref(thepix, 0) == NULL) {
		fprintf(stderr, "ERROR (readxy) - out of memory at field %u\n", 0);
		free_xyarray(thepix);
		return NULL;
	}
	int jj;
	for (jj = 0;jj < numxy;jj++) {
		double tmp1, tmp2;
		fscanf(fid, "%lf %lf\n", &tmp1, &tmp2);
		xy_setx(xya_ref(thepix, 0), jj, tmp1);
		xy_sety(xya_ref(thepix, 0), jj, tmp2);
	}
		
	return thepix;
}
