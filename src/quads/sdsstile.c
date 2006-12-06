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

#include "starutil.h"
#include "mathutil.h"
#include "rdlist.h"

#define OPTIONS "x:y:X:Y:w:h:s:S:N:"

extern char *optarg;
extern int optind, opterr, optopt;


static void addstar(float* fluximg, int x, int y, int W, int H) {
	/*
	  int dx[] = { -1,  0,  1, -1,  0,  1, -2, -2, -2,  2,  2,  2 };
	  int dy[] = { -2, -2, -2,  2,  2,  2, -1,  0,  1, -1,  0,  1 };
	*/
	/*
	  int dx[] = {  0, -1, -2,  1,  2,  1,  2, -1, -2 };
	  int dy[] = {  0, -1, -2,  1,  2, -1, -2,  1,  2 };
	*/
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

typedef unsigned char bool;
#define FALSE 0
#define TRUE 1

int main(int argc, char *argv[]) {
    int argchar;
	bool gotx, goty, gotX, gotY, gotw, goth, gots, gotS;
	double x0=0.0, x1=0.0, y0=0.0, y1=0.0;
	int w=0, h=0;
	char fn[256];
	int i;

	char* sdss_file_template = "/p/learning/stars/SDSS_FIELDS/sdssfield%02i.rd.fits";

	double px0, py0, px1, py1;
	double pixperx, pixpery;

	int filenum = -1;
	int fieldnum = -1;
	rdlist* rdls;
	double* radec;
	int Nstars;

	int pixelmargin = 4;
	int N = 0;

	gotx = goty = gotX = gotY = gotw = goth = gots = gotS = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'N':
			N = atoi(optarg);
			break;
		case 's':
			filenum = atoi(optarg);
			gots = TRUE;
			break;
		case 'S':
			fieldnum = atoi(optarg);
			gotS = TRUE;
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

	if (!(gotx && goty && gotX && gotY && gotw && goth && gots && gotS) ||
		(filenum == -1) || (fieldnum == -1)) {
		fprintf(stderr, "Invalid inputs.\n");
		exit(-1);
	}

	fprintf(stderr, "X range: [%g, %g] degrees\n", x0, x1);
	fprintf(stderr, "Y range: [%g, %g] degrees\n", y0, y1);
	fprintf(stderr, "File %i, Field %i.\n", filenum, fieldnum);

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

	sprintf(fn, sdss_file_template, filenum);
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

	if (N && (N < Nstars)) {
		Nstars = N;
		fprintf(stderr, "Keeping %i stars.\n", Nstars);
	}

	radec = malloc(Nstars * 2 * sizeof(double));
	if (rdlist_read_entries(rdls, fieldnum, 0, Nstars, radec)) {
		fprintf(stderr, "Failed to read RDLS file.\n");
		free(radec);
		exit(-1);
	}

	fprintf(stderr, "Got %i stars.\n", Nstars);

	{
		double querylow[2], queryhigh[2];
		double wraplow[2], wraphigh[2];
		float* img;
		float xscale, yscale;
		int Noob;
		int Nib;
		bool wrapra;
		double xorigin, yorigin;
		/*
		  Since we draw stars as little icons, we need to expand the search
		  range slightly...
		 */
		double xmargin = (double)pixelmargin / pixperx;
		double ymargin = (double)pixelmargin / pixpery;

		fprintf(stderr, "Expanding xy by margins (%g, %g)\n", xmargin, ymargin);

		wrapra = (px1 > 1.0);

		xorigin = px0;
		yorigin = py0;

		if (wrapra) {
			querylow[0] = px0 - xmargin;
			queryhigh[0] = 1.0;
			querylow[1] = py0 - ymargin;
			queryhigh[1] = py1 + ymargin;
			wraplow[0] = 0.0;
			wraphigh[0] = px1 - 1.0 + xmargin;
			wraplow[1] = py0 - ymargin;
			wraphigh[1] = py1 + ymargin;
		} else {
			querylow[0] = px0 - xmargin;
			querylow[1] = py0 - ymargin;
			queryhigh[0] = px1 + xmargin;
			queryhigh[1] = py1 + ymargin;
			// quell gcc warnings
			wraphigh[0] = wraphigh[1] = wraplow[0] = 0.0;
		}

		img = calloc(w*h*3, sizeof(float));
		if (!img) {
			fprintf(stderr, "Failed to allocate flux image.\n");
			exit(-1);
		}

		xscale = pixperx;
		yscale = pixpery;

		Noob = 0;
		Nib = 0;

		for (i=0; i<Nstars; i++) {
			double xs, ys;
			double ra, dec;
			int ix, iy;
			ra = radec[i*2 + 0];
			dec = radec[i*2 + 1];
			// RA/DEC values in FITS files are in degrees.
			xs = ra / 360.0;
			ys = (M_PI + asinh(tan(deg2rad(dec)))) / (2.0 * M_PI);

			if (!((xs >= querylow[0] && xs <= queryhigh[0] &&
				   ys >= querylow[1] && ys <= queryhigh[1]) ||
				  (wrapra &&
				   xs >= wraplow[0] && xs <= wraphigh[0] &&
				   ys >= wraplow[0] && ys <= wraphigh[1]))) {
				Noob++;
				continue;
			}
			if (xs < querylow[0]) {
				ix = (int)rint((xs + 1.0 - xorigin) * xscale);
			} else {
				ix = (int)rint((xs - xorigin) * xscale);
			}
			if (ix + pixelmargin < 0 || ix - pixelmargin >= w) {
				Noob++;
				continue;
			}
			// flip vertically
			iy = h - (int)rint((ys - yorigin) * yscale);
			if (iy + pixelmargin < 0 || iy - pixelmargin >= h) {
				Noob++;
				continue;
			}
			Nib++;

			addstar(img, ix, iy, w, h);
		}

		fprintf(stderr, "%i stars outside image bounds.\n", Noob);
		fprintf(stderr, "%i stars inside image bounds.\n", Nib);

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
