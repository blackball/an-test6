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

#undef TRUE
#include "an-bool.h"

#include "starutil.h"
#include "mathutil.h"
#include "rdlist.h"

#define OPTIONS "x:y:X:Y:w:h:N:f:F:"

extern char *optarg;
extern int optind, opterr, optopt;

static void addstar(float* fluximg, int x, int y, int W, int H) {
	int dx[] = {  0, -1, -2,  1,  2,  1,  2, -1, -2,
				  1,  0, -1,  2,  3,  2,  3,  0, -1 };
	int dy[] = {  0, -1, -2,  1,  2, -1, -2,  1,  2,
				  0, -1, -2,  1,  2, -1, -2,  1,  2 };
	float rflux, gflux, bflux;
	int i;
	rflux = 255.0;
	gflux = 200.0;
	bflux = 0.0;
	for (i=0; i<sizeof(dx)/sizeof(int); i++) {
		if ((x + dx[i] < 0) || (x + dx[i] >= W)) continue;
		if ((y + dy[i] < 0) || (y + dy[i] >= H)) continue;
		fluximg[3*((y+dy[i])*W+(x+dx[i])) + 0] += rflux;
		fluximg[3*((y+dy[i])*W+(x+dx[i])) + 1] += gflux;
		fluximg[3*((y+dy[i])*W+(x+dx[i])) + 2] += bflux;
	}
}

int main(int argc, char *argv[]) {
    int argchar;
	bool gotx, goty, gotX, gotY, gotw, goth;
	char* filename = NULL;
	double x0=0.0, x1=0.0, y0=0.0, y1=0.0;
	int w=0, h=0;
	char fn[256];
	int i;
	int fieldnum = 0;

	char* rdls_dir = "/p/learning/stars/GOOGLE_MAPS_RDLS/";

	double px0, py0, px1, py1;
	double pixperx, pixpery;

	xylist* rdls;
	double* rdvals;
	int Nstars;

	int pixelmargin = 4;
	int N = 0;

	gotx = goty = gotX = gotY = gotw = goth = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'N':
			N = atoi(optarg);
			break;
		case 'F':
			fieldnum = atoi(optarg);
			break;
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

	if (!(gotx && goty && gotX && gotY && gotw && goth) ||
		!filename) {
		fprintf(stderr, "Invalid inputs.\n");
		exit(-1);
	}

	fprintf(stderr, "X range: [%g, %g] degrees\n", x0, x1);
	fprintf(stderr, "Y range: [%g, %g] degrees\n", y0, y1);
	fprintf(stderr, "File %s, field %i.\n", filename, fieldnum);

	if (strstr(filename, "..")) {
		fprintf(stderr, "Filename is not allowed to contain \"..\".\n");
		exit(-1);
	}

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

	if (px0 < 0.0) {
		px0 += 1.0;
		px1 += 1.0;
		fprintf(stderr, "Wrapping X around to projected range [%g, %g]\n", px0, px1);
	}

	snprintf(fn, sizeof(fn), "%s/%s", rdls_dir, filename);
	fprintf(stderr, "Opening file: %s\n", fn);
	rdls = rdlist_open(fn);
	if (!rdls) {
		fprintf(stderr, "Failed to open RDLS file.\n");
		exit(-1);
	}
	Nstars = rdlist_n_entries(rdls, fieldnum);
	if (Nstars == -1) {
		fprintf(stderr, "Failed to read RDLS file.\n");
		exit(-1);
	}

	if (N && N < Nstars)
		Nstars = N;

	rdvals = malloc(Nstars * 2 * sizeof(double));
	if (rdlist_read_entries(rdls, fieldnum, 0, Nstars, rdvals)) {
		fprintf(stderr, "Failed to read RDLS file.\n");
		free(rdvals);
		exit(-1);
	}

	fprintf(stderr, "Got %i stars.\n", Nstars);

	{
		bool wrapra;
		/*
		  Since we draw stars as little icons, we need to expand the search
		  range slightly...
		 */
		double xmargin = (double)pixelmargin / pixperx;
		double ymargin = (double)pixelmargin / pixpery;
		double xorigin, yorigin;
		float* img;
		float xscale, yscale;

		img = calloc(w*h*3, sizeof(float));
		if (!img) {
			fprintf(stderr, "Failed to allocate flux image.\n");
			exit(-1);
		}

		xorigin = px0;
		yorigin = py0;
		xscale = pixperx;
		yscale = pixpery;
		wrapra = (px1 > 1.0);
		for (i=0; i<Nstars; i++) {
			double mx = rdvals[i*2+0] / 360.0;
			double my = (M_PI + asinh(tan(deg2rad(rdvals[i*2+1])))) / (2.0 * M_PI);
			int ix1, ix2=0, iy;
			bool ok;

			if (wrapra)
				ok = ((mx <= px1 - 1.0 + xmargin) ||
					  (mx >= px0 - xmargin));
			else
				ok = (mx >= px0 - xmargin) &&
					(mx <= px1 + xmargin);
			if (!ok) continue;
			ok = (my  >= py0 - ymargin) &&
				(my  <= py1 + ymargin);
			if (!ok) continue;

			ix1 = (int)rint((mx - xorigin) * xscale);
			if (wrapra)
				ix2 = (int)rint((mx + 1.0 - xorigin) * xscale);

			// ??
			//if ((ix + pixelmargin < 0) || (ix - pixelmargin) >= w) {
			iy = (int)rint((my - yorigin) * yscale);
			// flip.
			iy = h - iy;

			addstar(img, ix1, iy, w, h);
			if (wrapra)
				addstar(img, ix2, iy, w, h);
		}


		printf("P6 %d %d %d\n", w, h, 255);
		for (i=0; i<(w*h); i++) {
			unsigned char pix[3];
			pix[0] = img[3*i+0];
			pix[1] = img[3*i+1];
			pix[2] = img[3*i+2];
			fwrite(pix, 1, 3, stdout);
		}

		free(img);
	}
	return 0;
}
