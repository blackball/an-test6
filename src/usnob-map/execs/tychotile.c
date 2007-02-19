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

#include "starutil.h"
#include "kdtree.h"
#include "healpix.h"
#include "mathutil.h"
#include "merctree.h"
#include "bl.h"

#define max(a, b)  ((a)>(b)?(a):(b))
#define min(a, b)  ((a)<(b)?(a):(b))

#define OPTIONS "x:y:X:Y:w:h:l:f"

extern char *optarg;
extern int optind, opterr, optopt;

static void node_contained(kdtree_t* kd, int node, void* extra);
static void node_overlaps(kdtree_t* kd, int node, void* extra);
static void leaf_node(kdtree_t* kd, int node);
static void expand_node(kdtree_t* kd, int node);
static void addstar(double xp, double yp, merc_flux* flux);
					 
char* map_template  = "/home/gmaps/usnob-images/tycho-maps/tycho-zoom%i_%02i_%02i.png";
char* merc_file     = "/home/gmaps/usnob-images/tycho-merc/tycho.mkdt.fits";

float* fluximg;
float xscale, yscale;
int Noob;
int Nib;
int w=0, h=0;

// The current query region
double *qlo, *qhi;
// The lower-left corner of the image.
double xorigin, yorigin;
// The merctree.
merctree* merc;

int main(int argc, char *argv[]) {
    int argchar;
	bool gotx, goty, gotX, gotY, gotw, goth;
	double x0=0.0, x1=0.0, y0=0.0, y1=0.0;
	int i;
	double lines = 0.0;
	double px0, py0, px1, py1;
	double pixperx, pixpery;
	double xzoom, yzoom;
	int zoomlevel;
	int xpix0, xpix1, ypix0, ypix1;
	bool forcetree = FALSE;
	char fn[256];

	gotx = goty = gotX = gotY = gotw = goth = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'f':
			forcetree = TRUE;
			break;
		case 'l':
			lines = atof(optarg);
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

	if (!(gotx && goty && gotX && gotY && gotw && goth)) {
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

	if (px0 < 0.0) {
		px0 += 1.0;
		px1 += 1.0;
		fprintf(stderr, "Wrapping X around to projected range [%g, %g]\n", px0, px1);
	}

	xpix0 = px0 * pixperx;
	ypix0 = py0 * pixpery;
	xpix1 = xpix0 + w;
	ypix1 = ypix0 + h;

	fprintf(stderr, "Pixel positions: (%i,%i), (%i,%i) vs (%i,%i)\n", xpix0, ypix0, xpix0+w, ypix0+h, xpix1, ypix1);

	if (!forcetree && (zoomlevel <= 2)) {
		char buff[1024];

		// set filename so that correct tile is returned
		sprintf(fn, map_template, zoomlevel, (ypix0+1)/256, (xpix0+1)/256);

		fprintf(stderr, "Loading image from file %s.\n", fn);
		FILE *fp = fopen(fn, "r");

		// write file to stdout
		for (;;) {
			int nr = fread(buff, 1, sizeof(buff), fp);
			if (!nr) {
				if (feof(fp))
					break;
				if (ferror(fp)) {
					fprintf(stderr, "Error reading from file %s: %s\n", fn, strerror(errno));
					break;
				}
			}
			if (fwrite(buff, 1, nr, stdout) != nr) {
				fprintf(stderr, "Error writing: %s\n", strerror(errno));
				break;
			}
		}
		return 0;
	}

	{
		double querylow[2], queryhigh[2];
		double wraplow[2], wraphigh[2];
		bool wrapra;

		wrapra = (px1 > 1.0);
		if (wrapra) {
			querylow [0] = px0;
			queryhigh[0] = 1.0;
			querylow [1] = py0;
			queryhigh[1] = py1;
			wraplow  [0] = 0.0;
			wraphigh [0] = px1 - 1.0;
			wraplow  [1] = py0;
			wraphigh [1] = py1;
		} else {
			querylow [0] = px0;
			querylow [1] = py0;
			queryhigh[0] = px1;
			queryhigh[1] = py1;
		}

		fluximg = calloc(w*h*3, sizeof(float));
		if (!fluximg) {
			fprintf(stderr, "Failed to allocate flux image.\n");
			exit(-1);
		}

		xscale = pixperx;
		yscale = pixpery;
		Noob = 0;
		Nib = 0;

		fprintf(stderr, "Query: x:[%g,%g] y:[%g,%g]\n",
				querylow[0], queryhigh[0], querylow[1], queryhigh[1]);
		if (wrapra)
			fprintf(stderr, "Query': x:[%g,%g] y:[%g,%g]\n",
					wraplow[0], wraphigh[0], wraplow[1], wraphigh[1]);


		fprintf(stderr, "Opening file %s...\n", merc_file);
		merc = merctree_open(fn);
		if (!merc) {
			fprintf(stderr, "Failed to open merctree %s\n", merc_file);
			exit(-1);
		}

		xorigin = qlo[0];
		yorigin = qlo[1];
		qlo = querylow;
		qhi = queryhigh;
		kdtree_nodes_contained(merc->tree, qlo, qhi,
							   node_contained, node_overlaps, NULL);
	
		if (wrapra) {
			xorigin = qlo[0] - 1.0;
			qlo = wraplow;
			qhi = wraphigh;
			kdtree_nodes_contained(merc->tree, qlo, qhi,
								   node_contained, node_overlaps, NULL);
		}
		merctree_close(merc);


		fprintf(stderr, "%i points outside image bounds.\n", Noob);
		fprintf(stderr, "%i points inside image bounds.\n", Nib);

		{
			float rmax, bmax, nmax;
			float rscale, bscale, nscale;
			float sat = 4.0;
			rmax = bmax = nmax = 0.0;
			for (i=0; i<(w*h); i++) {
				fluximg[3*i+0] = pow(fluximg[3*i+0], 0.25);
				fluximg[3*i+1] = pow(fluximg[3*i+1], 0.25);
				fluximg[3*i+2] = pow(fluximg[3*i+2], 0.25);
				if (fluximg[3*i + 0] > rmax) rmax = fluximg[3*i + 0];
				if (fluximg[3*i + 1] > bmax) bmax = fluximg[3*i + 1];
				if (fluximg[3*i + 2] > nmax) nmax = fluximg[3*i + 2];
			}

			rscale = 255.0 / rmax * sat;
			bscale = 255.0 / bmax * sat;
			nscale = 255.0 / nmax * sat;

			printf("P6 %d %d %d\n", w, h, 255);
			for (i=0; i<(w*h); i++) {
				unsigned char pix[3];
				// fluximg stored the channels in RBN order.
				// we want to make N->red, R->green, B->blue
				pix[0] = min(255, fluximg[3*i + 2] * nscale);
				pix[1] = min(255, fluximg[3*i + 0] * rscale);
				pix[2] = min(255, fluximg[3*i + 1] * bscale);
				fwrite(pix, 1, 3, stdout);
			}
		}

		free(fluximg);
		return 0;
	}
}

// Non-leaf nodes that are fully contained within the query rectangle.
static void node_contained(kdtree_t* kd, int node, void* extra) {
	expand_node(kd, node);
}

// Leaf nodes that overlap the query rectangle.
static void node_overlaps(kdtree_t* kd, int node, void* extra) {
	leaf_node(kd, node);
}

static void expand_node(kdtree_t* kd, int node) {
	int xp0, xp1, yp0, yp1;
	int D = 2;
	double bblo[D], bbhi[D];

	// check if this whole box fits inside a pixel.
	kdtree_get_bboxes(kd, node, bblo, bbhi);
	xp0 = (int)floor(xscale * (bblo[0] - xorigin));
	xp1 = (int) ceil(xscale * (bbhi[0] - xorigin));
	if (xp1 - xp0 <= 1) {
		yp0 = (int)floor(yscale * (bblo[1] - yorigin));
		yp1 = (int) ceil(yscale * (bbhi[1] - yorigin));
		if (yp1 - yp0 <= 1) {
			// This node fits inside a single pixel of the output image.
			// use bblo as the point.
			addstar(bblo[0], bblo[1], &(merc->stats[node].flux));
			return;
		}
	}

	if (KD_IS_LEAF(kd, node)) {
		leaf_node(kd, node);
		return;
	}

	expand_node(kd,  KD_CHILD_LEFT(node));
	expand_node(kd, KD_CHILD_RIGHT(node));
}

static void leaf_node(kdtree_t* kd, int node) {
	int k;
	int L, R;

	L = kdtree_left(kd, node);
	R = kdtree_right(kd, node);

	for (k=L; k<=R; k++) {
		double pt[2];
		merc_flux* flux;

		kdtree_copy_data_double(kd, k, 1, pt);
		flux = merc->flux + k;
		addstar(pt[0], pt[1], flux);
	}
}

static void addstar(double xp, double yp,
					merc_flux* flux) {
	int dx[] = { -1,  0,  1,  0,  0 };
	int dy[] = {  0,  0,  0, -1,  1 };
	float scale[] = { 1, 1, 1, 1, 1 };
	int i;
	int x, y;
	int mindx = -1;
	int maxdx = 1;
	int mindy = -1;
	int maxdy = 1;

	x = (int)rint((xp - xorigin) * xscale);
	if (x+maxdx < 0 || x+mindx >= w) {
		Noob++;
		return;
	}
	// flip vertically
	y = h - (int)rint((yp - qlo[1]) * yscale);
	if (y+maxdy < 0 || y+mindy >= h) {
		Noob++;
		return;
	}
	Nib++;

	for (i=0; i<sizeof(dx)/sizeof(int); i++) {
		if ((x + dx[i] < 0) || (x + dx[i] >= w)) continue;
		if ((y + dy[i] < 0) || (y + dy[i] >= h)) continue;
		fluximg[3*(y*w + x) + 0] += flux->rflux * scale[i];
		fluximg[3*(y*w + x) + 1] += flux->bflux * scale[i];
		fluximg[3*(y*w + x) + 2] += flux->nflux * scale[i];
	}
}

