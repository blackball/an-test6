#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

/**
   This program gets called by "tile.php" in response to a client requesting a map
   tile.  The coordinates of the tile are specified as a range of RA and DEC values.

   The RA coordinates are passed as    -x <lower-RA> -X <upper-RA>
   The DEC coordinates are             -y <lower-DEC> -Y <upper-DEC>
   The width and height in pixels are  -w <width> -h <height>
*/

#define OPTIONS "x:y:X:Y:w:h:"

// Converts from RA in radians to Mercator X coordinate in [0, 1].
static double ra2mercx(double ra) {
	return ra / (2.0 * M_PI);
}
// Converts from Mercator X coordinate [0, 1] to RA in radians.
static double mercx2ra(double x) {
	return x * (2.0 * M_PI);
}
// Converts from DEC in radians to Mercator Y coordinate in [0, 1].
static double dec2mercy(double dec) {
	return 0.5 + (asinh(tan(dec)) / (2.0 * M_PI));
}
// Converts from Mercator Y coordinate [0, 1] to DEC in radians.
static double mercy2dec(double y) {
	return atan(sinh((y - 0.5) * (2.0 * M_PI)));
}

// Converts degrees to radians.
static double deg2rad(double r) {
	return r * M_PI / 180.0;
}

#define FALSE 0
#define TRUE  1

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	int gotx, goty, gotX, gotY, gotw, goth;
	double x0=0.0, x1=0.0, y0=0.0, y1=0.0;
	int w=0, h=0;
	int i;
	double px0, py0, px1, py1;
	double pixperx, pixpery;
	double xperpixel, yperpixel;
	double xzoom, yzoom;
	int zoomlevel;
	unsigned char* img;
	int ix, iy;

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
		fprintf(stderr, "Invalid inputs: need ");
		if (!gotx) fprintf(stderr, "-x ");
		if (!gotX) fprintf(stderr, "-X ");
		if (!goty) fprintf(stderr, "-y ");
		if (!gotY) fprintf(stderr, "-Y ");
		if (!gotw) fprintf(stderr, "-w ");
		if (!goth) fprintf(stderr, "-h ");
		exit(-1);
	}

	if (w > 1024 || h > 1024) {
		fprintf(stderr, "Width or height too large (limit 1024)\n");
		exit(-1);
	}

	fprintf(stderr, "X range: [%g, %g] degrees\n", x0, x1);
	fprintf(stderr, "Y range: [%g, %g] degrees\n", y0, y1);

	// Mercator projected coordinates
	px0 =  ra2mercx(deg2rad(x0));
	px1 =  ra2mercx(deg2rad(x1));
	py0 = dec2mercy(deg2rad(y0));
	py1 = dec2mercy(deg2rad(y1));

	fprintf(stderr, "Projected X range: [%g, %g]\n", px0, px1);
	fprintf(stderr, "Projected Y range: [%g, %g]\n", py0, py1);

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
	xperpixel = 1.0 / pixperx;
	yperpixel = 1.0 / pixpery;
	fprintf(stderr, "X,Y pixel scale: %g, %g.\n", pixperx, pixpery);
	xzoom = pixperx / 256.0;
	yzoom = pixpery / 256.0;
	fprintf(stderr, "X,Y zoom %g, %g\n", xzoom, yzoom);
	zoomlevel = (int)rint(log(xzoom) / log(2.0));
	fprintf(stderr, "Zoom level %i.\n", zoomlevel);

	// The Google Maps client treat RA as going from -180 to +180; we prefer to
	// think of it going from 0 to 360.  If the lower-RA value is negative, wrap
	// it around...
	if (px0 < 0.0) {
		px0 += 1.0;
		px1 += 1.0;
		fprintf(stderr, "Wrapping X around to projected range [%g, %g]\n", px0, px1);
	}

	// Allocate a black image.
	img = calloc(3 * w * h, 1);

	// draw a nice border in white.
	for (i=0; i<2; i++) {
		if (!i)
			// left side
			ix = 2;
		else
			// right side
			ix = w-3;
		for (iy=2; iy<h-2; iy++) {
			img[3 * (iy*w + ix) + 0] = 255;
			img[3 * (iy*w + ix) + 1] = 255;
			img[3 * (iy*w + ix) + 2] = 255;
		}
	}
	for (i=0; i<2; i++) {
		if (!i)
			// top
			iy = 2;
		else
			// bottom
			iy = h-3;
		for (ix=2; ix<w-2; ix++) {
			img[3 * (iy*w + ix) + 0] = 255;
			img[3 * (iy*w + ix) + 1] = 255;
			img[3 * (iy*w + ix) + 2] = 255;
		}
	}

	// fill the middle with a gradient with red on the X axis and blue on the Y.
	for (iy=3; iy<h-3; iy++)
		for (ix=3; ix<w-3; ix++) {
			img[3 * (iy * w + ix) + 0] = (int)(255.0 * (px0 + xperpixel * ix));
			img[3 * (iy * w + ix) + 1] = 0;
			img[3 * (iy * w + ix) + 2] = (int)(255.0 * (py1 - yperpixel * iy));
		}

	// write PPM on stdout.
	printf("P6 %d %d %d\n", w, h, 255);
	fwrite(img, 1, 3*h*w, stdout);

	free(img);

	return 0;
}
