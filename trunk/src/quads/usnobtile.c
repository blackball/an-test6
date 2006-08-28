#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>

//#include "usnob_fits.h"
#include "an_catalog.h"
#include "starutil.h"
#include "healpix.h"
#include "mathutil.h"

#define OPTIONS "x:y:X:Y:w:h:"

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	bool gotx, goty, gotX, gotY, gotw, goth;
	double x0, x1, y0, y1;
	int w, h;
	double xyz0[3];
	double xyz1[3];
	double center[3];
	double r2;
	uint hp;
	int Nside = 9;
	//char fntemplate = "/h/42/dstn/local2/USNOB10/fits/usnob10_hp%03i.fits";
	char* fntemplate = "/h/42/dstn/local/AN/an_hp%03i.fits";
	char fn[256];
	//usnob_fits* usnob;
	an_catalog* cat;
	int i, N;
	int valid;
	uchar* image;
	double scale;
	//uchar maxval;

	gotx = goty = gotX = gotY = gotw = goth = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
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

	image = calloc(w*h, 1);
	if (!image) {
		fprintf(stderr, "Failed to allocate image.\n");
		exit(-1);
	}

	x0 = deg2rad(x0);
	x1 = deg2rad(x1);
	y0 = deg2rad(y0);
	y1 = deg2rad(y1);
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

	//usnob = usnob_fits_open(fn);
	//N = usnob_fits_count_entries(usnob);

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
		//fprintf(stderr, "uv=(%f, %f)\n", u, v);
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
