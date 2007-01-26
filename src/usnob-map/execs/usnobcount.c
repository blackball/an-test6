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
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "ppm.h"
#include "pnm.h"

#include "an_catalog.h"
#include "starutil.h"
#include "healpix.h"
#include "mathutil.h"
#include "bl.h"

#define OPTIONS "r:d:w:h:z:"

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	bool gotr, gotd, gotw, goth, gotz;
	double ra=0.0, dec=0.0;
	int w=0, h=0, zoomlevel=0;

	gotr = gotd = gotw = goth = gotz = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'r':
			ra = atof(optarg);
			gotr = TRUE;
			break;
		case 'd':
			dec = atof(optarg);
			gotd = TRUE;
			break;
		case 'z':
			zoomlevel = atoi(optarg);
			gotz = TRUE;
			break;
		case 'w':
			w = atoi(optarg);
			gotw = TRUE;
			break;
		case 'h':
			h = atoi(optarg);
			goth = TRUE;
			break;
		}

	if (!(gotr && gotd && gotz && gotw && goth)) {
		fprintf(stderr, "Invalid inputs.\n");
		exit(-1);
	}

	fprintf(stderr, "RA,DEC [%g,%g] degrees\n", ra, dec);
	fprintf(stderr, "Zoom level %i.\n", zoomlevel);
	fprintf(stderr, "Pixel size (%i,%i).\n", w, h);

	{
		// projected position of center
		double px, py;
		double xlo, xhi, ylo, yhi;
		double nobjs;
		double zoomval;
		char str[256];
		int digits;
		int i, outind;

		px = ra / 360.0;
		if (px < 0.0)
			px += 1.0;
		py = (M_PI + asinh(tan(deg2rad(dec)))) / (2.0 * M_PI);
		zoomval = pow(2.0, (double)(zoomlevel-1));
		xlo = px - (0.5 * w / (double)(zoomval * 256.0));
		xhi = px + (0.5 * w / (double)(zoomval * 256.0));
		ylo = py - (0.5 * h / (double)(zoomval * 256.0));
		yhi = py + (0.5 * h / (double)(zoomval * 256.0));
		fprintf(stderr, "Projected ranges: x [%g, %g], y [%g, %g]\n",
				xlo, xhi, ylo, yhi);
		if (ylo < 0.0)
			ylo = 0.0;
		if (yhi > 1.0)
			yhi = 1.0;

		nobjs = (xhi - xlo) * (yhi - ylo) * 1.0e9;
		sprintf(str, "%lli", (long long int)rint(nobjs));
		// add commas
		digits = strlen(str);
		outind = digits + (digits-1)/3;
		str[outind] = '\0';
		outind--;
		for (i=0; i<digits; i++) {
			if (i && !(i%3)) {
				str[outind] = ',';
				outind--;
			}
			str[outind] = str[digits-(i+1)];
			outind--;
		}
		printf("%s\n", str);
	}


	return 0;
}
