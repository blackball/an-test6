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

#include "starutil.h"
#include "starkd.h"
#include "kdtree.h"
#include "healpix.h"
#include "mathutil.h"
#include "bl.h"

#define OPTIONS "x:y:X:Y:w:h:f:"

extern char *optarg;
extern int optind, opterr, optopt;

static void addstar(uchar* img, int x, int y, int W, int H,
					uchar r, uchar g, uchar b) {
	/*
	  int dx[] = { -1,  0,  1, -1,  0,  1, -2, -2, -2,  2,  2,  2 };
	  int dy[] = { -2, -2, -2,  2,  2,  2, -1,  0,  1, -1,  0,  1 };
	*/
	int dx[] = { -1,  0,  1,  2, -1,  0,  1,  2,
				 -2, -2, -2, -2,  3,  3,  3,  3 };
	//-2, -2,  3,  3 };
	int dy[] = { -2, -2, -2, -2,  3,  3,  3,  3,
				 -1,  0,  1,  2, -1,  0,  1,  2 };
	//-2,  3, -2,  3 };
	/*
	  int dx[] = { -1,  0,  1,  0,  0 };
	  int dy[] = {  0,  0,  0, -1,  1 };
	*/
	int i;
	for (i=0; i<sizeof(dx)/sizeof(int); i++) {
		if ((x + dx[i] < 0) || (x + dx[i] >= W)) continue;
		if ((y + dy[i] < 0) || (y + dy[i] >= H)) continue;
		img[3*((y+dy[i])*W+(x+dx[i])) + 0] = r;
		img[3*((y+dy[i])*W+(x+dx[i])) + 1] = g;
		img[3*((y+dy[i])*W+(x+dx[i])) + 2] = b;
	}
}

int main(int argc, char *argv[]) {
    int argchar;
	bool gotx, goty, gotX, gotY, gotw, goth;
	double x0=0.0, x1=0.0, y0=0.0, y1=0.0;
	int w=0, h=0;

	char* filename = NULL;
	char* basedir = "/home/gmaps/gmaps-indexes/";

	char fn[256];
	int i;

	double px0, py0, px1, py1;
	double pixperx, pixpery;
	double xzoom, yzoom;
	int zoomlevel;
	int xpix0, xpix1, ypix0, ypix1;

	int pixelmargin = 4;

	uchar* img;
	float xscale, yscale;
	int Noob;
	int Nib;
	startree* skdt;
	bool wrapra;
	double query[3];
	double maxd2;
	double xmargin;
	double ymargin;
	double xorigin, yorigin;

	double xlo, xhi, ylo, yhi, xlo2, xhi2;

	kdtree_qres_t* res;
	int j;

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

	if (px0 < 0.0) {
		px0 += 1.0;
		px1 += 1.0;
		fprintf(stderr, "Wrapping X around to projected range [%g, %g]\n", px0, px1);
	}

	xpix0 = px0 * pixperx;
	ypix0 = py0 * pixpery;
	xpix1 = px1 * pixperx;
	ypix1 = py1 * pixpery;

	fprintf(stderr, "Pixel positions: (%i,%i), (%i,%i) vs (%i,%i)\n", xpix0, ypix0, xpix0+w, ypix0+h, xpix1, ypix1);

	xpix1 = xpix0 + w;
	ypix1 = ypix0 + h;

	xorigin = px0;
	yorigin = py0;
	wrapra = (px1 > 1.0);
	xmargin = (double)pixelmargin / pixperx;
	ymargin= (double)pixelmargin / pixpery;

	xlo = px0 - xmargin;
	xhi = px1 + xmargin;
	ylo = py0 - ymargin;
	yhi = py1 + ymargin;

	if (wrapra) {
		xlo2 = 0.0;
		xhi2 = xhi - 1.0;
		xhi = 1.0;
	} else {
		xlo2 = xhi2 = 0.0;
	}

	img = calloc(w*h*3, sizeof(uchar));
	if (!img) {
		fprintf(stderr, "Failed to allocate  image.\n");
		exit(-1);
	}

	xscale = pixperx;
	yscale = pixpery;

	Noob = 0;
	Nib = 0;

	{
		double p0[3], p1[3];
		radec2xyzarr(x0, y0, p0);
		radec2xyzarr(x1, y1, p1);
		star_midpoint(query, p0, p1);
		maxd2 = 0.25 * distsq(p0, p1, 3);
	}

	snprintf(fn, sizeof(fn), "%s%s.skdt.fits", basedir, filename);
	fprintf(stderr, "Opening file %s...\n", fn);
	skdt = startree_open(fn);
	if (!skdt) {
		fprintf(stderr, "Failed to open startree.\n");
		exit(-1);
	}

	res = kdtree_rangesearch_options(skdt->tree, query, maxd2, 0);
	if (res)
		fprintf(stderr, "%i results.\n", res->nres);

	for (j=0; res && j<res->nres; j++) {
		double ra, dec;
		double* xyz;
		double px, py;
		int ix, iy;
		xyz = res->results.d + j*3;
		xyz2radec(xyz[0], xyz[1], xyz[2], &ra, &dec);
		px = ra / (2.0 * M_PI);
		py = (M_PI + asinh(tan(dec))) / (2.0 * M_PI);

		if (!( (py >= ylo) && (py <= yhi) &&
			   ( ((px >= xlo) && (px <= xhi)) ||
				 (wrapra && (px >= xlo2) && (px <= xhi2)) ) ))
			continue;

		ix = (int)rint((px - xorigin) * xscale);
		if (ix + pixelmargin < 0 ||
			ix - pixelmargin >= w)
			continue;
		iy = h - (int)rint((py - yorigin) * yscale);
		if (iy + pixelmargin < 0 ||
			iy - pixelmargin >= h)
			continue;
		//addstar(img, ix, iy, w, h, 0x90, 0xa8, 255);
		addstar(img, ix, iy, w, h, 255, 0, 0);
		Nib++;
	}

	kdtree_free_query(res);
	startree_close(skdt);

	fprintf(stderr, "%i stars inside image bounds.\n", Nib);

	printf("P6 %d %d %d\n", w, h, 255);
	fwrite(img, 1, 3*w*h, stdout);
	free(img);
	return 0;
}
