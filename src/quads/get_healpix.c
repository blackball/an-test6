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

#include "healpix.h"
#include "starutil.h"
#include "mathutil.h"

#define OPTIONS "hdN:np"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s [-d] ra dec\n"
		   "     (-d means values are in degree; by default they're in radians)\n"
		   "  %s x y z\n\n"
		   "  [-N nside]  (default 1)\n"
		   "  [-n]: print neighbours\n"
		   "  [-p]: project position within healpix\n"
		   "If your values are negative, add \"--\" in between \"-d\" (if you're using it) and \"ra\" or \"x\".\n\n",
		   progname, progname);
}

int main(int argc, char** args) {
    int healpix = -1;
    double argvals[argc];
    int i;
    bool degrees = FALSE;
    int nargs;
    int c;
	int Nside = 1;
	bool neighbours = FALSE;
	bool project = FALSE;
	double xyz[3];

    if (argc == 1) {
		print_help(args[0]);
		exit(0);
    }

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'd':
			degrees = TRUE;
			break;
		case 'N':
			Nside = atoi(optarg);
			break;
		case 'n':
			neighbours = TRUE;
			break;
		case 'p':
			project = TRUE;
		}
    }

    nargs = argc - optind;
    if (!((nargs == 2) || (nargs == 3))) {
		print_help(args[0]);
		exit(-1);
    }

    for (i=0; i<nargs; i++) {
		argvals[i] = atof(args[optind + i]);
    }

    if (nargs == 2) {
		double ra = argvals[0];
		double dec = argvals[1];

		if (degrees) {
			ra  *= M_PI / 180.0;
			dec *= M_PI / 180.0;
		}

		if (Nside > 1)
			healpix = radectohealpix_nside(ra, dec, Nside);
		else
			healpix = radectohealpix(ra, dec);
	
		printf("(RA, DEC) = (%g, %g) degrees\n", ra*180/M_PI, dec*180/M_PI);
		printf("(RA, DEC) = (%g, %g) radians\n", ra, dec);
		printf("Healpix=%i\n", healpix);

		radec2xyzarr(ra, dec, xyz);

    } else if (nargs == 3) {
		double x = argvals[0];
		double y = argvals[1];
		double z = argvals[2];

		if (Nside > 1)
			healpix = xyztohealpix_nside(x, y, z, Nside);
		else
			healpix = xyztohealpix(x, y, z);

		printf("(x, y, z) = (%g, %g, %g)\n", x, y, z);
		printf("Healpix=%i\n", healpix);

		xyz[0] = x;
		xyz[1] = y;
		xyz[2] = z;
    }

	if (neighbours) {
		uint neigh[8];
		uint nneigh;
		nneigh = healpix_get_neighbours_nside(healpix, neigh, Nside);
		printf("Neighbours=[ ");
		for (i=0; i<nneigh; i++)
			printf("%i ", neigh[i]);
		printf("]\n");
	}

	if (project) {
		double origin[3];
		double vx[3];
		double vy[3];
		double normal[3];
		double proj1[3];
		double proj2[3];
		double max1, max2;
		int d;
		double dot1, dot2;
		double bdydist;
		double* bdydir;
		double d2;

		healpix_to_xyzarr_lex(0.0, 0.0, healpix, Nside, origin);
		healpix_to_xyzarr_lex(1.0, 0.0, healpix, Nside, vx);
		healpix_to_xyzarr_lex(0.0, 1.0, healpix, Nside, vy);
		for (d=0; d<3; d++) {
			vx[d] -= origin[d];
			vy[d] -= origin[d];
		}
		cross_product(vx, vy, normal);
		cross_product(normal, vx, proj1);
		cross_product(vy, normal, proj2);
		max1 = max2 = 0.0;
		for (d=0; d<3; d++) {
			max1 += vy[d] * proj1[d];
			max2 += vx[d] * proj2[d];
		}

		dot1 = dot2 = 0.0;
		for (d=0; d<3; d++) {
			dot1 += proj1[d] * (xyz[d] - origin[d]);
			dot2 += proj2[d] * (xyz[d] - origin[d]);
		}

		printf("Projects to [%g, %g]\n", dot1/max1, dot2/max2);

		printf("Healpix scale is %g arcsec.\n", 60*healpix_side_length_arcmin(Nside));

		// which boundary is it closest to?
		bdydir = vx;
		if (dot1/max1 < 0.5)
			bdydist = dot1/max1;
		else
			bdydist = 1.0 - dot1/max1;

		if (dot2/max2 < bdydist) {
			bdydist = dot2/max2;
			bdydir = vy;
		} else if ((1.0 - dot2/max2) < bdydist) {
			bdydist = 1.0 - dot2/max2;
			bdydir = vy;
		}

		d2 = bdydir[0]*bdydir[0] + bdydir[1]*bdydir[1] + bdydir[2]*bdydir[2];
		d2 *= bdydist*bdydist;

		printf("Closest boundary %g arcsec away.\n", distsq2arcsec(d2));

	}


    return 0;
}
