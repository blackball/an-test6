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
#include <sys/mman.h>
#include <math.h>

#include "starutil.h"
#include "mathutil.h"
#include "boilerplate.h"
#include "xylist.h"

const char* OPTIONS = "h";

void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "\nUsage: %s <xyls-file>\n"
			"\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int argchar;
	char* progname = args[0];
	char** inputfiles = NULL;
	int ninputfiles = 0;
	xylist* xyls;
	dl* xy;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1) {
		switch (argchar) {
		case 'h':
		default:
			printHelp(progname);
			exit(-1);
		}
	}
	if (optind < argc) {
		ninputfiles = argc - optind;
		inputfiles = args + optind;
	}
	if (ninputfiles != 1) {
		printHelp(progname);
		exit(-1);
	}

	xyls = xylist_open(inputfiles[0]);
	if (!xyls) {
		fprintf(stderr, "Failed to open xyls file %s.\n", inputfiles[0]);
		exit(-1);
	}

	xy = xylist_get_field(xyls, 0);
	if (!xy) {
		fprintf(stderr, "Failed to get XYLS field.\n");
		exit(-1);
	}

	{
		double xmax, xmin, ymax, ymin;
		double diag;
		int i;
		xmax = ymax = -HUGE_VAL;
		xmin = ymin =  HUGE_VAL;
		for (i=0; i<(dl_size(xy)/2); i++) {
			double x = dl_get(xy, 2*i);
			double y = dl_get(xy, 2*i+1);
			if (x > xmax) xmax = x;
			if (x < xmin) xmin = x;
			if (y > ymax) ymax = y;
			if (y < ymin) ymin = y;
		}
 
		diag = hypot(xmax-xmin, ymax-ymin);

		printf("x_min %g\n", xmin);
		printf("x_max %g\n", xmax);
		printf("y_min %g\n", ymin);
		printf("y_max %g\n", ymax);
		printf("width %g\n", xmax-xmin);
		printf("height %g\n", ymax-ymin);
		printf("x_center %g\n", (xmin + xmax) / 2.0);
		printf("y_center %g\n", (ymin + ymax) / 2.0);
		printf("diag %g\n", diag);
	}

	dl_free(xy);
	xylist_close(xyls);
	return 0;
}
