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
#include <assert.h>

#include "an-bool.h"
#include "starkd.h"
#include "kdtree.h"
#include "starutil.h"
#include "healpix.h"
#include "mathutil.h"
#include "bl.h"
#include "permutedsort.h"
#include "qidxfile.h"
#include "quadfile.h"

#define OPTIONS "r:d:w:h:z:f:m:"

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char fn[256];
	int i, j;
	startree* skdt;
	qidxfile* qidx;
	quadfile* qfile;

	bool gotr, gotd, gotw, goth, gotz;
	double ra=0.0, dec=0.0;
	int w=0, h=0, zoomlevel=0;

	char* filename = NULL;
	char* basedir = "/home/gmaps/gmaps-indexes/";
	double query[3];
	double maxd2;
	double x0, x1, y0, y1;
	double px0, py0, px1, py1;
	double pixperx, pixpery;
	double xzoom, yzoom;

	kdtree_qres_t* res;
	int maxquads = 0;
	int totalquads;

	gotr = gotd = gotw = goth = gotz = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'm':
			maxquads = atoi(optarg);
			break;
		case 'f':
			filename = optarg;
			break;
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

	if (!(gotr && gotd && gotz && gotw && goth && filename)) {
		fprintf(stderr, "Invalid inputs.\n");
		exit(-1);
	}

	fprintf(stderr, "RA,DEC [%g,%g] degrees\n", ra, dec);
	fprintf(stderr, "Zoom level %i.\n", zoomlevel);
	fprintf(stderr, "Pixel size (%i,%i).\n", w, h);

	{
		// projected position of center
		double px, py;
		double zoomval;
		double xlo, xhi, ylo, yhi;

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

		px0 = xlo;
		px1 = xhi;
		py0 = ylo;
		py1 = yhi;

		x0 = px0 * (2.0 * M_PI);
		x1 = px1 * (2.0 * M_PI);
		y0 = atan(sinh(py0 * (2.0 * M_PI) - M_PI));
		y1 = atan(sinh(py1 * (2.0 * M_PI) - M_PI));
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

	if (zoomlevel < 5) {
		fprintf(stderr, "Zoomed out too far!\n");
		printf("<quads>\n");
		printf("</quads>\n");
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
	qidx = qidxfile_open(fn);
	if (!qidx) {
		fprintf(stderr, "Failed to open quadidx file.\n");
		exit(-1);
	}

	snprintf(fn, sizeof(fn), "%s%s.quad.fits", basedir, filename);
	fprintf(stderr, "Opening file %s...\n", fn);
	qfile = quadfile_open(fn);
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
		fprintf(stderr, "%i stars.\n", res->nres);

	printf("<quads>\n");
	totalquads = 0;
	for (j=0; res && j<res->nres; j++) {
		uint* quads;
		uint nquads;
		int k, s;
		int dimquads = quadfile_dimquads(qfile);

		qidxfile_get_quads(qidx, res->inds[j], &quads, &nquads);
		totalquads += nquads;
		if (maxquads && (totalquads > maxquads)) {
			nquads = totalquads - maxquads;
		}
		for (k=0; k<nquads; k++) {
			double midab[3];
			uint stars[dimquads];
			double theta[dimquads];
			int perm[dimquads];
			double xyzs[3*dimquads];
			quadfile_get_stars(qfile, quads[k], stars);
			for (s=0; s<4; s++)
				startree_get(skdt, stars[s], xyzs + 3*s);

			star_midpoint(midab, xyzs + 3*0, xyzs + 3*1);
			// sort the points by angle from the midpoint so that the
			// polygon doesn't self-intersect.
			for (s=0; s<dimquads; s++) {
				double x, y;
				bool ok;
				ok = star_coords(xyzs + 3*s, midab, &x, &y);
				assert(ok);
				theta[s] = atan2(y, x);
				perm[s] = s;
			}
			permuted_sort_set_params(theta, sizeof(double), compare_doubles);
			permuted_sort(perm, dimquads);

			printf("  <quad");
			for (i=0; i<dimquads; i++) {
				double ra, dec;
				xyzarr2radec(xyzs + 3*perm[i], &ra, &dec);
				ra  = rad2deg(ra);
				dec = rad2deg(dec);
				printf(" ra%i=\"%g\" dec%i=\"%g\"", i, ra, i, dec);
			}
			printf("/>\n");
		}
		if (maxquads && (totalquads >= maxquads)) {
			fprintf(stderr, "Exceeded %i quads\n", maxquads);
			break;
		}
	}
	printf("</quads>\n");

	fprintf(stderr, "Total number of quads: %i.\n", totalquads);

	startree_close(skdt);
	qidxfile_close(qidx);
	quadfile_close(qfile);
	return 0;
}

