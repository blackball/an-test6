#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "an_catalog.h"
#include "starutil.h"
#include "healpix.h"
#include "mathutil.h"
#include "bl.h"
#include "ppm.h"
#include "pnm.h"

#define OPTIONS "x:y:X:Y:w:h:l:"

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	bool gotx, goty, gotX, gotY, gotw, goth;
	double x0=0.0, x1=0.0, y0=0.0, y1=0.0;
	int w=0, h=0;
	double xyz0[3];
	double xyz1[3];
	double center[3];
	double r2;
	uint hp;
	int Nside = 9;
	char* fntemplate = "/h/42/dstn/local/AN/an_hp%03i.fits";
	char* map_template = "/h/42/dstn/local/maps/usnob-zoom%i.ppm";
	char fn[256];
	an_catalog* cat;
	int i, N;
	int valid;
	uchar* image;
	double scale;
	double lines = 0.0;

	double px0, py0, px1, py1;
	double xperpix, yperpix;
	double xzoom, yzoom;
	int zoomlevel;
	int xpix0, xpix1, ypix0, ypix1;

	gotx = goty = gotX = gotY = gotw = goth = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
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
	py0 = asinh(tan(y0));
	py1 = asinh(tan(y1));
	xperpix = fabs(px1 - px0) / (double)w;
	yperpix = fabs(py1 - py0) / (double)h;
	fprintf(stderr, "Projected X range: [%g, %g]\n", px0, px1);
	fprintf(stderr, "Projected Y range: [%g, %g]\n", py0, py1);
	fprintf(stderr, "X,Y pixel scale: %g, %g.\n", xperpix, yperpix);
	xzoom = 1.0 / (xperpix * 256.0);
	yzoom = 1.0 / (yperpix * 256.0 / (2.0 * M_PI));
	fprintf(stderr, "X,Y zoom %g, %g\n", xzoom, yzoom);
	if ((fabs(xzoom - rint(xzoom)) > 0.05) ||
		(fabs(xzoom - yzoom) > 0.05)) {
		fprintf(stderr, "Invalid zoom level.\n");
		exit(-1);
	}
	zoomlevel = (int)rint(log(xzoom) / log(2.0));
	fprintf(stderr, "Zoom level %i.\n", zoomlevel);

	if (px0 < 0.0) {
		px0 += 1.0;
		px1 += 1.0;
		fprintf(stderr, "Wrapping X around to projected range [%g, %g]\n", px0, px1);
	}

	xpix0 = px0 / xperpix;
	ypix0 = (-py1 + M_PI) / yperpix;
	xpix1 = px1 / xperpix;
	ypix1 = (-py0 + M_PI) / yperpix;

	fprintf(stderr, "Pixel positions: (%i,%i), (%i,%i) vs (%i,%i)\n", xpix0, ypix0, xpix0+w, ypix0+h, xpix1, ypix1);

	xpix1 = xpix0 + w;
	ypix1 = ypix0 + h;

	if (zoomlevel <= 5) {
		char fn[256];
		FILE* fin;
		int rows, cols, format;
		pixval maxval;
		off_t imgstart;
		unsigned char* map;
		size_t mapsize;
		unsigned char* img;
		int y;
		unsigned char* outimg;

		sprintf(fn, map_template, zoomlevel);
		fprintf(stderr, "Loading image from file %s.\n", fn);
		fin = pm_openr_seekable(fn);
		ppm_readppminit(fin, &cols, &rows, &maxval, &format);
		if (PNM_FORMAT_TYPE(format) != PPM_TYPE) {
			fprintf(stderr, "Map file must be PPM.\n");
			exit(-1);
		}
		if (maxval != 255) {
			fprintf(stderr, "Error: PPM maxval %i (not 255).\n", maxval);
			exit(-1);
		}
		imgstart = ftello(fin);
		mapsize = cols * rows * 3;
		map = mmap(NULL, mapsize, PROT_READ, /*MAP_SHARED*/MAP_PRIVATE, fileno(fin), 0);
		if (map == MAP_FAILED) {
			fprintf(stderr, "Failed to mmap image file.\n");
			exit(-1);
		}
		img = map + imgstart;

		/*
		  if (xpix1 < xpix0) {
		  int tmp = xpix0;
		  xpix0 = xpix1;
		  xpix1 = tmp;
		  fprintf(stderr, "Swapping xpix0 and xpix1.\n");
		  }
		  if (ypix1 < ypix0) {
		  int tmp = ypix0;
		  ypix0 = ypix1;
		  ypix1 = tmp;
		  fprintf(stderr, "Swapping ypix0 and ypix1.\n");
		  }
		  if (xpix0 < 0) {
		  fprintf(stderr, "Shifting xpix0 from %i up to 0.\n", xpix0);
		  xpix0 = 0;
		  }
		  if (ypix0 < 0) {
		  fprintf(stderr, "Shifting ypix0 from %i up to 0.\n", ypix0);
		  ypix0 = 0;
		  }
		  if (xpix0 >= cols) {
		  fprintf(stderr, "Shifting xpix0 from %i down to %i.\n", xpix0, cols-1);
		  xpix0 = cols-1;
		  }
		  if (ypix0 >= rows) {
		  fprintf(stderr, "Shifting ypix0 from %i down to %i.\n", ypix0, rows-1);
		  ypix0 = rows-1;
		  }
		  if (xpix1 < 0) {
		  fprintf(stderr, "Shifting xpix1 from %i up to 0.\n", xpix1);
		  xpix1 = 0;
		  }
		  if (ypix1 < 0) {
		  fprintf(stderr, "Shifting ypix1 from %i up to 0.\n", ypix1);
		  ypix1 = 0;
		  }
		  if (xpix1 >= cols) {
		  fprintf(stderr, "Shifting xpix1 from %i down to %i.\n", xpix1, cols-1);
		  xpix1 = cols-1;
		  }
		  if (ypix1 >= rows) {
		  fprintf(stderr, "Shifting ypix1 from %i down to %i.\n", ypix1, rows-1);
		  ypix1 = rows-1;
		  }
		  if ((xpix1 - xpix0) != w) {
		  fprintf(stderr, "Changing xpix1 to match image size %i.\n", w);
		  xpix1 = xpix0 + w;
		  }
		  if ((ypix1 - ypix0) != h) {
		  fprintf(stderr, "Changing ypix1 to match image size %i.\n", h);
		  ypix1 = ypix0 + h;
		  }
		  fprintf(stderr, "Pixel positions: (%i,%i), (%i,%i)\n", xpix0, ypix0, xpix1, ypix1);
		*/

		if (xpix0 < 0 || ypix0 < 0 || xpix1 > cols || ypix1 > rows) {
			fprintf(stderr, "Requested pixels (%i,%i) to (%i,%i) aren't within image bounds (0,0),(%i,%i)\n",
					xpix0, ypix0, xpix1, ypix1, cols, rows);
			exit(-1);
		}

		/*
		  for (y=ypix0; y<ypix1; y++) {
		  unsigned char* start = img + 3*((y * cols) + xpix0);
		  fwrite(start, 1, w*3, stdout);
		  }
		*/
		outimg = malloc(3 * w * h);
		for (y=ypix0; y<ypix1; y++) {
			unsigned char* start = img + 3*((y * cols) + xpix0);
			memcpy(outimg + 3*w*(y-ypix0), start, 3*w);
		}

		munmap(map, mapsize);

		if (lines != 0.0) {
			double rstart, rend, dstart, dend;
			double r, d;
			unsigned char linecolor[] = { 255, 0, 0 };
			double linealpha = 0.25;
			int c;
			int x;
			double x0wrap = (x0 < 0.0 ? 2.0*M_PI : 0.0);

			rstart = floor(rad2deg(x0 + x0wrap) / lines) * lines;
			rend   =  ceil(rad2deg(x1 + x0wrap) / lines) * lines;
			dstart = floor(rad2deg(y0) / lines) * lines;
			dend   =  ceil(rad2deg(y1) / lines) * lines;
			for (r=rstart; r<=rend; r+=lines) {
				int px;
				px = (int)rint(((deg2rad(r) - (x0 + x0wrap)) / (2.0*M_PI)) / xperpix);
				//fprintf(stderr, "RA %g: pix %i.\n", r, px);
				if (px < 0 || px >= w)
					continue;
				for (y=0; y<h; y++) {
					for (c=0; c<3; c++) {
						int ind = 3*((y*w) + px) + c;
						outimg[ind] = (unsigned char)(outimg[ind] * (1.0 - linealpha) + linecolor[c] * linealpha);
					}
				}
			}
			for (d=dstart; d<=dend; d+=lines) {
				int py;
				py = (int)rint((M_PI - asinh(tan(deg2rad(d)))) / yperpix) - ypix0;
				//fprintf(stderr, "DEC %g: pix %i.\n", d, py);
				if (py < 0 || py >= h)
					continue;
				for (x=0; x<w; x++) {
					for (c=0; c<3; c++) {
						int ind = 3*((py*w) + x) + c;
						outimg[ind] = (unsigned char)(outimg[ind] * (1.0 - linealpha) + linecolor[c] * linealpha);
					}
				}
			}
		}

		// output PPM.
		printf("P6 %d %d %d\n", w, h, 255);
		fwrite(outimg, 1, w*h*3, stdout);

		free(outimg);

		return 0;
	} else {
		//bool ra_wrap;
		int Nside = 9;
		int HP;
		int* hpimg;
		int hpimgsize = 256;
		int x, y, hp;
		int xstart, xend, ystart, yend;
		bool* hps;
		il* hplist;

		HP = 12 * Nside * Nside;

		hpimg = malloc(hpimgsize * hpimgsize * sizeof(int));
		for (y=0; y<hpimgsize; y++) {
			double xval, yval;
			double ra, dec;
			yval = (((double)y / (double)hpimgsize) * 2.0 * M_PI) - M_PI;
			dec = atan(sinh(yval));
			for (x=0; x<hpimgsize; x++) {
				xval = ((double)x / (double)hpimgsize);
				ra = xval * 2.0 * M_PI;
				hp = radectohealpix_nside(ra, dec, Nside);
				hpimg[y*hpimgsize + x] = hp;
			}
		}

		xstart = (int)floor(px0 * hpimgsize);
		xend   = (int)ceil (px1 * hpimgsize);
		ystart = (int)floor((py0 + M_PI) / (2.0*M_PI) * hpimgsize);
		yend   = (int)ceil ((py1 + M_PI) / (2.0*M_PI) * hpimgsize);

		//fprintf(stderr, "x [%i, %i], y [%i, %i]\n", xstart, xend, ystart, yend);

		hps = calloc(HP, sizeof(bool));

		for (y=ystart; y<=yend; y++) {
			if (y < 0 || y >= hpimgsize)
				continue;
			for (x=xstart; x<=xend; x++) {
				if (x < 0 || x >= hpimgsize)
					continue;
				hps[hpimg[y*hpimgsize + x]] = TRUE;
			}
		}

		free(hpimg);

		hplist = il_new(32);

		fprintf(stderr, "Healpixes: ");
		for (i=0; i<HP; i++) {
			if (hps[i]) {
				il_append(hplist, i);
				fprintf(stderr, "%i ", i);
			}
		}
		fprintf(stderr, "\n");

		free(hps);

		for (i=0; i<il_size(hplist); i++) {
			hp = il_get(hplist, i);
		}

		il_free(hplist);

		return 0;
	}

	image = calloc(w*h, 1);
	if (!image) {
		fprintf(stderr, "Failed to allocate image.\n");
		exit(-1);
	}

	radec2xyzarr(x0, y0, xyz0);
	radec2xyzarr(x1, y1, xyz1);
	star_midpoint(center, xyz0, xyz1);
	r2 = distsq(xyz0, xyz1, 3) / 4.0;
	{
		double ra, dec, arcsec;
		xyz2radec(center[0], center[1], center[2], &ra, &dec);
		ra  = rad2deg(ra);
		dec = rad2deg(dec);
		arcsec = distsq2arcsec(r2);
		fprintf(stderr, "Center (%f, %f), Radius %f arcsec.\n", ra, dec, arcsec);
	}

	hp = xyztohealpix_nside(center[0], center[1], center[2], Nside);
	sprintf(fn, fntemplate, hp);

	fprintf(stderr, "Healpix %i.\n", hp);

	// HACK here
	scale = (double)w / sqrt(r2);

	cat = an_catalog_open(fn);

	N = an_catalog_count_entries(cat);

	fprintf(stderr, "%i entries.\n", N);

	valid = 0;
	for (i=0; i<N; i++) {
		an_entry* entry;
		double ra, dec;
		double xyz[3];
		double u, v;
		int iu, iv;

		entry = an_catalog_read_entry(cat);
		ra  = deg2rad(entry->ra);
		dec = deg2rad(entry->dec);
		radec2xyzarr(ra, dec, xyz);
		if (distsq(xyz, center, 3) > r2)
			continue;
		star_coords(xyz, center, &u, &v);
		u *= scale;
		v *= scale;
		iu = (int)rint(u);
		iv = (int)rint(v);
		if ((iu < 0) || (iv < 0) || (iu >= w) || (iv >= h))
			continue;
		valid++;
		//image[iv * w + iu]++;
		image[iv * w + iu] = 255;
	}
	an_catalog_close(cat);

	fprintf(stderr, "%i entries kept.\n", valid);

	/*
	  maxval = 1;
	  for (i=0; i<(w*h); i++) {
	  if (image[i] > maxval)
	  maxval = image[i];
	  }
	  fprintf(stderr, "Maxval %i.\n", maxval);
	*/
	// Output PGM format
	printf("P5 %d %d %d\n", w, h, 255);
	fwrite(image, 1, w*h, stdout);

	/*
	  for (i=0; i<(w*h); i++)
	  putchar(image[i] * 255 / maxval);
	*/

	free(image);
	return 0;
}
