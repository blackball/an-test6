/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

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

#include "sip.h"
#include "sip_qfits.h"
#include "starutil.h"
#include "mathutil.h"
#include "boilerplate.h"

const char* OPTIONS = "h";

void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "\nUsage: %s <wcs-file>\n"
			"\n", progname);
}

static double ra2mercx(double ra) {
	return ra / 360.0;
}
static double dec2mercy(double dec) {
	return 0.5 + (asinh(tan(deg2rad(dec))) / (2.0 * M_PI));
}

/*
  #define min(a,b) (((a)<(b))?(a):(b))
  #define max(a,b) (((a)>(b))?(a):(b))
*/

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int argchar;
	char* progname = args[0];
	char** inputfiles = NULL;
	int ninputfiles = 0;
	sip_t wcs;
	int imw, imh;
	qfits_header* wcshead = NULL;
	double rac, decc;
	double det, T, A, parity, orient;

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

	wcshead = qfits_header_read(inputfiles[0]);
	if (!wcshead) {
		fprintf(stderr, "failed to read wcs file %s\n", inputfiles[0]);
		return -1;
	}

	if (!sip_read_header(wcshead, &wcs)) {
		fprintf(stderr, "failed to read WCS header from file %s.\n", inputfiles[0]);
		return -1;
	}

	imw = qfits_header_getint(wcshead, "IMAGEW", -1);
	imh = qfits_header_getint(wcshead, "IMAGEH", -1);
	if ((imw == -1) || (imh == -1)) {
		fprintf(stderr, "failed to find IMAGE{W,H} in WCS file.\n");
		return -1;
	}

	printf("cd11 %.12g\n", wcs.wcstan.cd[0][0]);
	printf("cd12 %.12g\n", wcs.wcstan.cd[0][1]);
	printf("cd21 %.12g\n", wcs.wcstan.cd[1][0]);
	printf("cd22 %.12g\n", wcs.wcstan.cd[1][1]);

	det = sip_det_cd(&wcs);
	parity = (det >= 0 ? 1.0 : -1.0);
	printf("det %.12g\n", det);
	printf("parity %i\n", (int)parity);
	printf("pixscale %.12g\n", 3600.0 * sqrt(fabs(det)));

	T = parity * wcs.wcstan.cd[0][0] + wcs.wcstan.cd[1][1];
	A = parity * wcs.wcstan.cd[1][0] - wcs.wcstan.cd[0][1];
	orient = -rad2deg(atan2(A, T));
	printf("orientation %.8g\n", orient);

	sip_pixelxy2radec(&wcs, imw/2, imh/2, &rac, &decc);

	printf("ra_center %.12g\n", rac);
	printf("dec_center %.12g\n", decc);

	// mercator
	printf("ra_center_merc %.8g\n", ra2mercx(rac));
	printf("dec_center_merc %.8g\n", dec2mercy(decc));

	qfits_header_destroy(wcshead);

	return 0;
}
