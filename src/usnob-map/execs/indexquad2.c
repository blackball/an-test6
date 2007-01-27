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

#include "an-bool.h"

#include "ppm.h"
#include "pnm.h"

#include "starkd.h"
#include "kdtree.h"
#include "starutil.h"
#include "healpix.h"
#include "mathutil.h"
#include "bl.h"
#include "permutedsort.h"
#include "qidxfile.h"
#include "quadfile.h"

#define OPTIONS "x:y:X:Y:w:h:f:"

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char fn[256];
	int i, j;
	startree* skdt;
	qidxfile* qidx;
	quadfile* qfile;

	bool gotx, goty, gotX, gotY, gotw, goth;
	double x0=0.0, x1=0.0, y0=0.0, y1=0.0;
	int w=0, h=0;
	char* filename = NULL;
	char* basedir = "/home/gmaps/gmaps-indexes/";
	double query[3];
	double maxd2;
	double px0, py0, px1, py1;
	double pixperx, pixpery;
	double xzoom, yzoom;
	int zoomlevel;

	kdtree_qres_t* res;

	gotx = goty = gotX = gotY = gotw = goth = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'f':
			filename = optarg;
			break;
		case 'x':
			x0 = atof(optarg);
			gotx = TRUE;
			break;
		case 'X':
			x1 = atof(optarg);
			goty = TRUE;
			break;
		case 'y':
			y0 = atof(optarg);
			gotX = TRUE;
			break;
		case 'Y':
			y1 = atof(optarg);
			gotY = TRUE;
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

	if (!(gotx && goty && gotX && gotY && gotw && goth && filename)) {
		fprintf(stderr, "Invalid inputs.\n");
		exit(-1);
	}

	fprintf(stderr, "X range: [%g, %g] degrees\n", x0, x1);
	fprintf(stderr, "Y range: [%g, %g] degrees\n", y0, y1);

	x0 = deg2rad(x0);
	x1 = deg2rad(x1);
	y0 = deg2rad(y0);
	y1 = deg2rad(y1);

	// Mercator projected position
	px0 = x0 / (2.0 * M_PI);
	px1 = x1 / (2.0 * M_PI);
	py0 = (M_PI + asinh(tan(y0))) / (2.0 * M_PI);
	py1 = (M_PI + asinh(tan(y1))) / (2.0 * M_PI);
	if (px1 < px0) {
		fprintf(stderr, "Error, px1 < px0 (%g < %g)\n", px1, px0);
		exit(-1);
	}
	if (py1 < py0) {
		fprintf(stderr, "Error, py1 < py0 (%g < %g)\n", py1, py0);
		exit(-1);
	}

	pixperx = (double)w / (px1 - px0);
	pixpery = (double)h / (py1 - py0);
	fprintf(stderr, "Projected X range: [%g, %g]\n", px0, px1);
	fprintf(stderr, "Projected Y range: [%g, %g]\n", py0, py1);
	fprintf(stderr, "X,Y pixel scale: %g, %g.\n", pixperx, pixpery);
	xzoom = pixperx / 256.0;
	yzoom = pixpery / 256.0;
	fprintf(stderr, "X,Y zoom %g, %g\n", xzoom, yzoom);
	{
		double fxzoom;
		double fyzoom;
		fxzoom = log(xzoom) / log(2.0);
		fyzoom = log(yzoom) / log(2.0);
		fprintf(stderr, "fzoom %g, %g\n", fxzoom, fyzoom);
	}
	zoomlevel = (int)rint(log(xzoom) / log(2.0));
	fprintf(stderr, "Zoom level %i.\n", zoomlevel);

	if (zoomlevel < 3) {
		fprintf(stderr, "Zoomed out too far!\n");
		printf("P6 %d %d %d\n", w, h, 255);
		for (i=0; i<(w*h); i++) {
			unsigned char pix[3];
			pix[0] = 128;
			pix[1] = 128;
			pix[2] = 128;
			fwrite(pix, 1, 3, stdout);
		}
		return 0;
	}

	snprintf(fn, sizeof(fn), "%s%s.skdt.fits", basedir, filename);
	fprintf(stderr, "Opening file %s...\n", fn);
	skdt = startree_open(fn);
	if (!skdt) {
		fprintf(stderr, "Failed to open startree.\n");
		exit(-1);
	}

	snprintf(fn, sizeof(fn), "%s%s.qidx.fits", basedir, filename);
	fprintf(stderr, "Opening file %s...\n", fn);
	qidx = qidxfile_open(fn, 0);
	if (!qidx) {
		fprintf(stderr, "Failed to open quadidx file.\n");
		exit(-1);
	}

	snprintf(fn, sizeof(fn), "%s%s.quad.fits", basedir, filename);
	fprintf(stderr, "Opening file %s...\n", fn);
	qfile = quadfile_open(fn, 0);
	if (!qfile) {
		fprintf(stderr, "Failed to open quad file.\n");
		exit(-1);
	}

	{
		double p0[3], p1[3];
		radec2xyzarr(x0, y0, p0);
		radec2xyzarr(x1, y1, p1);
		star_midpoint(query, p0, p1);
		maxd2 = 0.25 * distsq(p0, p1, 3);
	}

	res = kdtree_rangesearch_options(skdt->tree, query, maxd2, 0);
	if (res)
		fprintf(stderr, "%i results.\n", res->nres);

	printf("<quads>\n");
	for (j=0; res && j<res->nres; j++) {
		uint* quads;
		uint nquads;
		int k, s;

		qidxfile_get_quads(qidx, res->inds[j], &quads, &nquads);

		for (k=0; k<nquads; k++) {
			uint stars[4];
			double xyzs[12];
			double midab[3];
			double theta[4];
			int perm[4];
			quadfile_get_starids(qfile, quads[k], stars+0, stars+1, stars+2, stars+3);
			for (s=0; s<4; s++)
				startree_get(skdt, stars[s], xyzs + 3*s);

			star_midpoint(midab, xyzs + 3*0, xyzs + 3*1);
			// sort the points by angle from the midpoint so that the
			// polygon doesn't self-intersect.
			for (s=0; s<4; s++) {
				double x, y;
				star_coords(xyzs + 3*s, midab, &x, &y);
				theta[s] = atan2(y, x);
				perm[s] = s;
			}
			permuted_sort_set_params(theta, sizeof(double), compare_doubles);
			permuted_sort(perm, 4);

			printf("  <quad");
			for (i=0; i<4; i++) {
				double ra, dec;
				xyzarr2radec(xyzs + 3*perm[i], &ra, &dec);
				ra  = rad2deg(ra);
				dec = rad2deg(dec);
				printf(" ra%i=\"%g\" dec%i=\"%g\"", i, ra, i, dec);
			}
			printf("/>\n");
		}
	}
	printf("</quads>\n");

	startree_close(skdt);
	qidxfile_close(qidx);
	quadfile_close(qfile);
	return 0;
}

