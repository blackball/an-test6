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

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "matchfile.h"

static const char* OPTIONS = "h";

static void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s <input matchfile>\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;


void printxyzasradec(double* xyz)
{
	double ra = rad2deg(xy2ra(xyz[0],xyz[1]));
	double dec = rad2deg(z2dec(xyz[2]));
	printf("ra=%lf, dec=%lf", ra, dec);
}

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];

	char* infn = NULL;
	matchfile* matchin;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'h':
		default:
			printHelp(progname);
			exit(-1);
		}
	}


	if ((optind+2) < argc) {
		printHelp(progname);
		exit(-1);
	}

	infn = argv[optind];

	if (!infn) {
		fprintf(stderr, "Must specify input match file.\n");
		printHelp(progname);
		exit(-1);
	}

	matchin = matchfile_open(infn);
	if (!matchin) {
		fprintf(stderr, "Failed to read input matchfile %s.\n", infn);
		exit(-1);
	}

	for (;;) {
		MatchObj* mo;
		mo = matchfile_buffered_read_match(matchin);
		if (!mo)
			break;
		if (!mo->transform_valid) {
			printf("MatchObject transform isn't valid.\n");
			continue;
		}

		printf("min corner: ");
		printxyzasradec(mo->sMin);
		printf(", max corner: ");
		printxyzasradec(mo->sMax);

		if (mo->parity)
			printf(" with parity swapped");
		printf("\n");
	}

	return 0;
}

