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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "boilerplate.h"
#include "solvedfile.h"

const char* OPTIONS = "ho:e";

void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "\nUsage: %s -o <output-file> <input-file> ...\n"
			"    [-e]: no error if file no found (assume empty)\n"
			"\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int argchar;
	char* progname = args[0];
	char** inputfiles = NULL;
	int ninputfiles = 0;
	int i;
	char* outfile = NULL;
	int N;
	char* solved;
	int noerr = 0;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1) {
		switch (argchar) {
		case 'o':
			outfile = optarg;
			break;
		case 'e':
			noerr = 1;
			break;
		case 'h':
		default:
			printHelp(progname);
			exit(-1);
		}
	}
	if (optind < argc) {
		ninputfiles = argc - optind;
		inputfiles = args + optind;
	} else {
		printHelp(progname);
		exit(-1);
	}

	N = 0;
	for (i=0; i<ninputfiles; i++) {
		int n = solvedfile_getsize(inputfiles[i]);
		if (n == -1) {
			if (!noerr) {
				fprintf(stderr, "Failed to get size of input file %s.\n", inputfiles[i]);
				exit(-1);
			}
		}
		if (n > N) N = n;
	}

	if (solvedfile_setsize(outfile, N)) {
		fprintf(stderr, "Failed to set size of output file.\n");
		exit(-1);
	}

	solved = calloc(N, 1);

	for (i=0; i<ninputfiles; i++) {
		il* slist;
		int j;
		slist = solvedfile_getall(inputfiles[i], 0, N, 0);
		for (j=0; j<il_size(slist); j++)
			solved[il_get(slist, j)] = (char)1;
		il_free(slist);
	}
	for (i=0; i<N; i++) {
		if (solved[i])
			if (solvedfile_set(outfile, i)) {
				fprintf(stderr, "Failed to set value in output file.\n");
				exit(-1);
			}
	}

	free(solved);
	return 0;
}
