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

#define OPTIONS "x:y:X:Y:w:h:H:"

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
	bool gotx, goty, gotX, gotY, gotw, goth, gotH;
	double x0=0.0, x1=0.0, y0=0.0, y1=0.0;
	int w=0, h=0;
	char* skdt_template = "/p/learning/stars/INDEXES/sdss-18/sdss-18_%02i.skdt.fits";
	char fn[256];
	int i;

	double px0, py0, px1, py1;
	double pixperx, pixpery;
	double xzoom, yzoom;
	int zoomlevel;
	int xpix0, xpix1, ypix0, ypix1;
	int healpix = 0;

	int pixelmargin = 4;

	gotx = goty = gotX = gotY = gotw = goth = gotH = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'H':
			healpix = atoi(optarg);
			gotH = TRUE;
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

	if (!(gotx && goty && gotX && gotY && gotw && goth && gotH)) {
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
	/*
	  if ((fabs(xzoom - rint(xzoom)) > 0.05) ||
	  (fabs(xzoom - yzoom) > 0.05)) {
	  fprintf(stderr, "Invalid zoom level.\n");
	  exit(-1);
	  }
	*/
	zoomlevel = (int)rint(log(xzoom) / log(2.0));
	fprintf(stderr, "Zoom level %i.\n", zoomlevel);

	if (zoomlevel < 7) {
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

	{
		//int Nside = 1;
		il* hplist;
		uchar* img;
		float xscale, yscale;
		int Noob;
		int Nib;
		startree* skdt;
		real querylow[2], queryhigh[2];
		real wraplow[2], wraphigh[2];
		bool wrapra;
		real query[3];
		real maxd2;
		double xmargin = (double)pixelmargin / pixperx;
		double ymargin = (double)pixelmargin / pixpery;
		double xorigin, yorigin;

		xorigin = px0;
		yorigin = py0;
		wrapra = (px1 > 1.0);
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
			wraphigh[0] = wraphigh[1] = wraplow[0] = wraplow[1] = 0.0;
		}

		hplist = il_new(12);

		img = calloc(w*h*3, sizeof(uchar));
		if (!img) {
			fprintf(stderr, "Failed to allocate  image.\n");
			exit(-1);
		}

		xscale = pixperx;
		yscale = pixpery;

		Noob = 0;
		Nib = 0;

		il_append(hplist, healpix);

		/*
		  {
		  double x,y,z;
		  double ra,dec;
		  real bblo[2], bbhi[2];
		  double minx, miny, maxx, maxy, minxnz;
		  double dx[] = { 0.0, 0.0, 1.0, 1.0 };
		  double dy[] = { 0.0, 1.0, 1.0, 0.0 };
		  int j;

		  i = healpix;

		  minx = miny = minxnz = 1e100;
		  maxx = maxy = -1e100;
		  for (j=0; j<4; j++) {
		  healpix_to_xyz(dx[j], dy[j], i, Nside, &x, &y, &z);
		  xyz2radec(x, y, z, &ra, &dec);
		  x = ra / (2.0 * M_PI);
		  y = (asinh(tan(dec)) + M_PI) / (2.0 * M_PI);
		  if (x > maxx) maxx = x;
		  if (x < minx) minx = x;
		  if ((x != 0.0) && (x < minxnz)) minxnz = x;
		  if (y > maxy) maxy = y;
		  if (y < miny) miny = y;
		  }

		  if (minx == 0.0) {
		  if ((1.0 - minxnz) < (maxx - minx)) {
		  // RA wrap-around.
		  minx = 0.0;
		  maxx = 1.0;
		  }
		  }

		  bblo[0] = minx;
		  bbhi[0] = maxx;
		  bblo[1] = miny;
		  bbhi[1] = maxy;

		  if (kdtree_do_boxes_overlap(bblo, bbhi, querylow, queryhigh, 2) ||
		  (wrapra && kdtree_do_boxes_overlap(bblo, bbhi, wraplow, wraphigh, 2))) {
		  fprintf(stderr, "HP %i overlaps: x:[%g,%g], y:[%g,%g]\n",
		  i, bblo[0], bbhi[0], bblo[1], bbhi[1]);
		  il_append(hplist, i);
		  }
		  }
		*/
		{
			double racenter = (x0 + x1)*0.5;
			double deccenter = (y0 + y1)*0.5;
			double p0[3], p1[3];
			radec2xyzarr(racenter, deccenter, query);
			radec2xyzarr(x0, y0, p0);
			radec2xyzarr(x1, y1, p1);
			maxd2 = 0.25 * distsq(p0, p1, 3);
		}

		for (i=0; i<il_size(hplist); i++) {
			kdtree_qres_t* res;
			int hp;
			int j;

			hp = il_get(hplist, i);
			sprintf(fn, skdt_template, hp);
			fprintf(stderr, "Opening file %s...\n", fn);
			skdt = startree_open(fn);
			if (!skdt) {
				fprintf(stderr, "Failed to open startree for healpix %i.\n", hp);
				continue;
			}

			res = kdtree_rangesearch_options(skdt->tree, query, maxd2, 0);
			if (res)
				fprintf(stderr, "%i results.\n", res->nres);

			for (j=0; res && j<res->nres; j++) {
				double ra, dec;
				double* xyz;
				double px, py;
				int ix, iy;
				xyz = res->results + j*3;
				xyz2radec(xyz[0], xyz[1], xyz[2], &ra, &dec);
				px = ra / (2.0 * M_PI);
				py = (M_PI + asinh(tan(dec))) / (2.0 * M_PI);
				if (!(((px >= querylow[0]) && (px <= queryhigh[0]) &&
					   (py >= querylow[1]) && (py <= queryhigh[1])) ||
					  (wrapra &&
					   (px >= wraplow[0]) && (px <= wraphigh[0]) &&
					   (py >= wraplow[1]) && (py <= wraphigh[1]))))
					continue;
				ix = (int)rint((px - xorigin) * xscale);
				iy = h - (int)rint((py - yorigin) * yscale);
				if (ix + pixelmargin < 0 ||
					ix - pixelmargin >= w ||
					iy + pixelmargin < 0 ||
					iy - pixelmargin >= h)
					continue;
				//addstar(img, ix, iy, w, h, 255, 255, 255);
				//addstar(img, ix, iy, w, h, 255, 255, 255);
				addstar(img, ix, iy, w, h, 0x90, 0xa8, 255);
				Nib++;
			}

			kdtree_free_query(res);
			startree_close(skdt);
		}

		fprintf(stderr, "%i stars inside image bounds.\n", Nib);

		il_free(hplist);

		printf("P6 %d %d %d\n", w, h, 255);
		fwrite(img, 1, 3*w*h, stdout);
		free(img);
		return 0;
	}
}
