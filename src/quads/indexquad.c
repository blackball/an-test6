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
#include <limits.h>
#include <sys/mman.h>

#define TRUE 1
#include "ppm.h"
#include "pnm.h"
#undef bool

#include "starkd.h"
#include "kdtree/kdtree.h"
#include "kdtree/kdtree_macros.h"
#include "kdtree/kdtree_access.h"
#include "starutil.h"
#include "healpix.h"
#include "mathutil.h"
#include "bl.h"
#include "permutedsort.h"

#define OPTIONS "q:H:"

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* skdt_template = "/p/learning/stars/INDEXES/sdss-18/sdss-18_%02i.skdt.fits";
	char fn[256];
	int i, j;
	int healpix = -1;
	startree* skdt;
	il* quads = il_new(16);

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'H':
			healpix = atoi(optarg);
			break;
		case 'q':
			il_append(quads, atoi(optarg));
			break;
		}

	if (healpix < 0 || healpix >= 12  || !il_size(quads) || (il_size(quads) % 4)) {
		fprintf(stderr, "Invalid inputs.\n");
		exit(-1);
	}

	fprintf(stderr, "Healpix %i\n", healpix);
	fprintf(stderr, "Quads: ");
	for (i=0; i<il_size(quads); i++)
		fprintf(stderr, "%i ", il_get(quads, i));
	fprintf(stderr, "\n");

	sprintf(fn, skdt_template, healpix);
	fprintf(stderr, "Opening file %s...\n", fn);
	skdt = startree_open(fn);
	if (!skdt) {
		fprintf(stderr, "Failed to open startree for healpix %i.\n", healpix);
		exit(-1);
	}

	printf("<quads>\n");
	for (j=0; j<il_size(quads)/4; j++) {
		double xyzABCD[12];
		double xyz0[3];
		double theta[4];
		int inds[4];
		int perm[4];
		for (i=0; i<4; i++) {
			inds[i] = il_get(quads, j*4 + i);
			if (inds[i] < 0 || inds[i] >= skdt->tree->ndata) {
				fprintf(stderr, "Invalid starid %i\n", inds[i]);
				exit(-1);
			}
			startree_get(skdt, inds[i], xyzABCD + 3*i);
		}
		star_midpoint(xyz0, xyzABCD + 3*0, xyzABCD + 3*1);
		for (i=0; i<4; i++) {
			double x, y;
			star_coords(xyzABCD + 3*i, xyz0, &x, &y);
			theta[i] = atan2(y, x);
			perm[i] = i;
		}
		permuted_sort_set_params(theta, sizeof(double), compare_doubles);
		permuted_sort(perm, 4);
		printf("  <quad");
		for (i=0; i<4; i++) {
			double ra, dec;
			double* xyz = xyzABCD + 3 * perm[i];
			xyz2radec(xyz[0], xyz[1], xyz[2], &ra, &dec);
			ra = rad2deg(ra);
			dec = rad2deg(dec);
			printf(" ra%i=\"%g\" dec%i=\"%g\"", i, ra, i, dec);
		}
		printf("/>\n");
	}
	printf("</quads>\n");

	startree_close(skdt);
	return 0;
}

