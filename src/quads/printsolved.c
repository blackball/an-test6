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

#include "starutil.h"
#include "mathutil.h"
#include "boilerplate.h"
#include "bl.h"
#include "solvedfile.h"

const char* OPTIONS = "hum:S";

void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "\nUsage: %s <solved-file> ...\n"
			"    [-u]: print UNsolved fields\n"
			"    [-m <max-field>]: for unsolved mode, max field number.\n"
			"    [-S]: for unsolved mode, use Sloan max field numbers, and assume the files are given in order.\n"
			"    [-w]: format for the wiki.\n"
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
	bool unsolved = FALSE;
	int maxfield = 0;
	bool sloan = FALSE;
	bool wiki = FALSE;

	int sloanmaxes[] = { 9978, 9980, 9974, 9974, 9965, 9971, 9965, 9979, 9978, 9979,
						 9981, 9978, 9981, 9977, 9973, 9977, 9981, 9977, 9972, 9975,
						 9981, 9980, 9980, 9975, 9974, 9970, 9978, 9979, 9979, 9978,
						 9981, 9971, 9983, 7318, 12 };

    while ((argchar = getopt (argc, args, OPTIONS)) != -1) {
		switch (argchar) {
		case 'w':
			wiki = TRUE;
			break;
		case 'S':
			sloan = TRUE;
			break;
		case 'u':
			unsolved = TRUE;
			break;
		case 'm':
			maxfield = atoi(optarg);
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

	for (i=0; i<ninputfiles; i++) {
		FILE* fid;
		int filesize;
		int j;
		int lim;
		il* list;

		fid = fopen(inputfiles[i], "rb");
		if (!fid) {
			fprintf(stderr, "Failed to open input file %s: %s\n",
					inputfiles[i], strerror(errno));
			exit(-1);
		}
		fseeko(fid, 0, SEEK_END);
		filesize = ftello(fid);
		fclose(fid);
		printf("File %s\n", inputfiles[i]);
		if (sloan && (i < (sizeof(sloanmaxes) / sizeof(int))))
			lim = imin(filesize, sloanmaxes[i]);
		else if (maxfield)
			lim = imin(maxfield, filesize);
		else
			lim = filesize;

		if (wiki)
			printf("|| %i || ", i+1);

		if (unsolved)
			list = solvedfile_getall(inputfiles[i], 0, lim-1, 0);
		else
			list = solvedfile_getall_solved(inputfiles[i], 0, lim-1, 0);

		if (!list) {
			fprintf(stderr, "Failed to get list of fields.\n");
			exit(-1);
		}
		for (j=0; j<il_size(list); j++)
			printf("%i ", il_get(list, j));
		il_free(list);

		if (wiki)
			printf(" ||");

		printf("\n");
	}

	return 0;
}
