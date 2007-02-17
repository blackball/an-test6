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
#include <math.h>

#include "starutil.h"
#include "boilerplate.h"
#include "rdlist.h"

const char* OPTIONS = "h";

void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "\nUsage: %s <rdls-file>\n"
			"\n", progname);
}

static double ra2mercx(double ra) {
	return ra / 360.0;
}
static double dec2mercy(double dec) {
	return 0.5 + (asinh(tan(deg2rad(dec))) / (2.0 * M_PI));
}
static double mercy2dec(double y) {
	return rad2deg(atan(sinh((y - 0.5) * (2.0 * M_PI))));
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int argchar;
	char* progname = args[0];
	char** inputfiles = NULL;
	int ninputfiles = 0;
	rdlist* rdls;
	dl* rd;

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

	rdls = rdlist_open(inputfiles[0]);
	if (!rdls) {
		fprintf(stderr, "Failed to open rdls file %s.\n", inputfiles[0]);
		exit(-1);
	}

	rd = rdlist_get_field(rdls, 0);
	if (!rd) {
		fprintf(stderr, "Failed to get RDLS field.\n");
		exit(-1);
	}

	{
		double ramax, ramin, decmax, decmin;
		double dx, dy;
		double maxd;
		int i;
		ramax = decmax = -1e100;
		ramin = decmin =  1e100;
		for (i=0; i<(dl_size(rd)/2); i++) {
			double ra  = dl_get(rd, 2*i);
			double dec = dl_get(rd, 2*i+1);
			if (ra > ramax) ramax = ra;
			if (ra < ramin) ramin = ra;
			if (dec > decmax) decmax = dec;
			if (dec < decmin) decmin = dec;
		}
		if (ramax - ramin > 180.0) {
			// probably wrapped around
			for (i=0; i<(dl_size(rd)/2); i++) {
				double ra  = dl_get(rd, 2*i);
				if (ra > 180) ra -= 360;
				if (ra > ramax) ramax = ra;
				if (ra < ramin) ramin = ra;
				/*
				  if (ra <= 180 && ra > ramax) ramax = ra;
				  if (ra > 180 ra < ramin) ramin = ra;
				*/
			}
		}

		printf("ra_min %g\n", ramin);
		printf("ra_max %g\n", ramax);
		printf("dec_min %g\n", decmin);
		printf("dec_max %g\n", decmax);
		printf("ra_center %g\n", (ramin + ramax) / 2.0);
		printf("dec_center %g\n", (decmin + decmax) / 2.0);

		// mercator
		printf("ra_center_merc %g\n", (ramin + ramax)/2.0);
		printf("dec_center_merc %g\n", mercy2dec((dec2mercy(decmin) + dec2mercy(decmax))/2.0));
		// mercator/gmaps zoomlevel.
		dx = (ra2mercx(ramax) - ra2mercx(ramin));
		dy = (dec2mercy(decmax) - dec2mercy(decmin));
		maxd = (dx > dy ? dx : dy);
		printf("zoom_merc %i\n", 1 + (int)floor(log(1.0 / maxd) / log(2.0)));
	}

	dl_free(rd);

	rdlist_close(rdls);

	return 0;
}
