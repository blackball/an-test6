#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <assert.h>
#include <png.h>
#include "qfits.h"
#include "sip.h"

/**
   This program gets called by "tile.php" in response to a client requesting a map
   tile.  The coordinates of the tile are specified as a range of RA and DEC values.
 
   The RA coordinates are passed as    -x <lower-RA> -X <upper-RA>
   The DEC coordinates are             -y <lower-DEC> -Y <upper-DEC>
   The width and height in pixels are  -w <width> -h <height>
   The WCS of the image
   as a FITS header chunk              -F <filename>
   The image to be projected           -f <filename>
 
   Note that if the WCS is not valid this WILL CRASH. Probably. At least for now.
*/

#define OPTIONS "x:y:X:Y:w:h:F:f:"

// fires an ALPHA png out stdout
void writeimage(unsigned char * img, int w, int h) 
{
	png_bytepp image_rows;
	int n;

	image_rows = malloc(sizeof(png_bytep)*h);
	for (n = 0; n < h; n++)
		image_rows[n] = (unsigned char *) img + 4*n*w;

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop png_info = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, stdout);
	png_set_filter(png_ptr, 0, PNG_FILTER_NONE);
	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);

	png_set_IHDR(png_ptr, png_info, w, h, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(png_ptr, png_info);

	png_write_image(png_ptr, image_rows);
	png_write_end(png_ptr, png_info);

	free(image_rows);

	png_destroy_write_struct(&png_ptr, &png_info);
}

// Converts from RA in radians to Mercator X coordinate in [0, 1].
static double ra2mercx(double ra)
{
	return ra / (2.0 * M_PI);
}
// Converts from Mercator X coordinate [0, 1] to RA in radians.
static double mercx2ra(double x)
{
	return x * (2.0 * M_PI);
}
// Converts from DEC in radians to Mercator Y coordinate in [0, 1].
static double dec2mercy(double dec)
{
	return 0.5 + (asinh(tan(dec)) / (2.0 * M_PI));
}
// Converts from Mercator Y coordinate [0, 1] to DEC in radians.
static double mercy2dec(double y)
{
	return atan(sinh((y - 0.5) * (2.0 * M_PI)));
}

// Converts degrees to radians.
static double deg2rad(double r)
{
	return r * M_PI / 180.0;
}
// Converts degrees to radians.
static double rad2deg(double d)
{
	return d * 180.0 / M_PI;
}

#define FALSE 0
#define TRUE  1

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[])
{
	int argchar;
	int gotx, goty, gotX, gotY, gotw, goth;
	double x0 = 0.0, x1 = 0.0, y0 = 0.0, y1 = 0.0;
	int w = 0, h = 0;
	int i, j;
	double px0, py0, px1, py1;
	double pixperx, pixpery;
	double xperpixel, yperpixel;
	double xzoom, yzoom;
	int zoomlevel;
	unsigned char* img;
	int ix, iy;

	char* imagefn=NULL;
	char* wcsfn=NULL;
	int gotF=FALSE;
	int gotf=FALSE;

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
		case 'f':
			imagefn = strdup(optarg);
			gotf = TRUE;
			break;
		case 'F':
			wcsfn = strdup(optarg);
			gotF = TRUE;
			break;
		}

	if (!(gotx && goty && gotX && gotY && gotw && goth && gotf && gotF)) {
		fprintf(stderr, "Invalid inputs: need ");
		if (!gotx)
			fprintf(stderr, "-x ");
		if (!gotX)
			fprintf(stderr, "-X ");
		if (!goty)
			fprintf(stderr, "-y ");
		if (!gotY)
			fprintf(stderr, "-Y ");
		if (!gotw)
			fprintf(stderr, "-w ");
		if (!goth)
			fprintf(stderr, "-h ");
		if (!gotf)
			fprintf(stderr, "-f ");
		if (!gotF)
			fprintf(stderr, "-F ");
		fprintf(stderr, "\n");
		exit( -1);
	}

	if (w > 1024 || h > 1024) {
		fprintf(stderr, "Width or height too large (limit 1024)\n");
		exit( -1);
	}

	fprintf(stderr, "VVV----------------------\n");
	fprintf(stderr, "VVV X range: [%g, %g] degrees\n", x0, x1);
	fprintf(stderr, "VVV Y range: [%g, %g] degrees\n", y0, y1);

	// Mercator projected coordinates
	px0 = ra2mercx(deg2rad(x0));
	px1 = ra2mercx(deg2rad(x1));
	py0 = dec2mercy(deg2rad(y0));
	py1 = dec2mercy(deg2rad(y1));

	fprintf(stderr, "VVV Projected X range: [%g, %g]\n", px0, px1);
	fprintf(stderr, "VVV Projected Y range: [%g, %g]\n", py0, py1);

	if (px1 < px0) {
		fprintf(stderr, "Error, px1 < px0 (%g < %g)\n", px1, px0);
		exit( -1);
	}
	if (py1 < py0) {
		fprintf(stderr, "Error, py1 < py0 (%g < %g)\n", py1, py0);
		exit( -1);
	}

	pixperx = (double)w / (px1 - px0); 
	pixpery = (double)h / (py1 - py0);
	xperpixel = 1.0 / pixperx;
	yperpixel = 1.0 / pixpery;
	fprintf(stderr, "VVV X,Y pixel scale: %g, %g.\n", pixperx, pixpery);
	xzoom = pixperx / 256.0;
	yzoom = pixpery / 256.0;
	fprintf(stderr, "X,Y zoom %g, %g\n", xzoom, yzoom);
	zoomlevel = (int)rint(log(xzoom) / log(2.0));
	fprintf(stderr, "Zoom level %i.\n", zoomlevel);

	// READ THE IMAGE
	// Note: this implementation is STUPID BEYOND WORDS. it is insane to
	// require ppm input, and insane to require re-reading the image for
	// every tile. nevertheless, slow working code is better than no code!
	FILE* f = fopen(imagefn, "r");
	int imw, imh;
	// THIS IS COMPLETELY DEPENDENT ON IMAGEMAGICK's 'convert' PPM OUTPUT!
	// The actual ppm 'standard' allows any whitespace between the tokens
	// P6 w h vpp data, but for fscanf we only accept one form, which is
	// what 'convert' outptus.
	fscanf(f, "P6");
	fprintf(stderr, "position after fscanf: %ld\n", ftell(f));
	fscanf(f, "%d %d\n255\n", &imw, &imh);
	fprintf(stderr, "position after fscanf: %ld\n", ftell(f));
	fprintf(stderr, "got imwid=%d, imheight=%d\n", imw, imh);
	unsigned char* imbuf = malloc(imw*imh*3);
//	fscanf(f, "P6\n%d %d\n255\n", &imw, &imh);
//	fscanf(f, "P6\n%d %d\n255\n", &imw, &imh);
//	fscanf(f, "P6\n%d %d\n255\n", &imw, &imh);
	fread(imbuf, 1, imw*imh*3, f);
	fprintf(stderr, "position after fscanf: %ld\n", ftell(f));

	// READ THE WCS
	// read wcs into tan structure. IGNORES SIP.
	tan_t wcs;
	fprintf(stderr, "wcsfn: %s\n",wcsfn);
	qfits_header* wcshead = qfits_header_read(wcsfn);
	wcs.crpix[0] = qfits_header_getdouble(wcshead, "CRPIX1",1e300);
	wcs.crpix[1] = qfits_header_getdouble(wcshead, "CRPIX2",1e300);
	wcs.crval[0] = qfits_header_getdouble(wcshead, "CRVAL1", 1e300);
	wcs.crval[1] = qfits_header_getdouble(wcshead, "CRVAL2", 1e300);
	wcs.cd[0][0] = qfits_header_getdouble(wcshead, "CD1_1", 1e300);
	wcs.cd[0][1] = qfits_header_getdouble(wcshead, "CD1_2", 1e300);
	wcs.cd[1][0] = qfits_header_getdouble(wcshead, "CD2_1", 1e300);
	wcs.cd[1][1] = qfits_header_getdouble(wcshead, "CD2_2", 1e300);

	fprintf(stderr, "wcs.crpix[0] =%lf\n", wcs.crpix[0]);
	fprintf(stderr, "wcs.crpix[1] =%lf\n", wcs.crpix[1]);
	fprintf(stderr, "wcs.crval[0] =%lf\n", wcs.crval[0]);
	fprintf(stderr, "wcs.crval[1] =%lf\n", wcs.crval[1]);
	fprintf(stderr, "wcs.cd[0][0] =%lf\n", wcs.cd[0][0]);
	fprintf(stderr, "wcs.cd[0][1] =%lf\n", wcs.cd[0][1]);
	fprintf(stderr, "wcs.cd[1][0] =%lf\n", wcs.cd[1][0]);
	fprintf(stderr, "wcs.cd[1][1] =%lf\n", wcs.cd[1][1]);


	// The Google Maps client treat RA as going from -180 to +180; we prefer to
	// think of it going from 0 to 360.  If the lower-RA value is negative, wrap
	// it around...
	//if (px0 < 0.0) {
	if (0) {
		px0 += 1.0;
		px1 += 1.0;
		fprintf(stderr, "Wrapping X around to projected range [%g, %g]\n", px0, px1);
	}

	// Allocate a black image.
	img = calloc(4 * w * h, 1);

	// draw a nice border in white.
	for (i = 0; i < 2; i++) {
		if (!i)
			// left side
			ix = 2;
		else
			// right side
			ix = w - 3;
		for (iy = 2; iy < h - 2; iy++) {
			img[3 * (iy*w + ix) + 0] = 255;
			img[3 * (iy*w + ix) + 1] = 255;
			img[3 * (iy*w + ix) + 2] = 255;
		}
	}
	for (i = 0; i < 2; i++) {
		if (!i)
			// top
			iy = 2;
		else
			// bottom
			iy = h - 3;
		for (ix = 2; ix < w - 2; ix++) {
			img[3 * (iy*w + ix) + 0] = 255;
			img[3 * (iy*w + ix) + 1] = 255;
			img[3 * (iy*w + ix) + 2] = 255;
		}
	}

	// fill the middle with a gradient with red on the X axis and blue on the Y.
	for (iy = 3; iy < h - 3; iy++)
		for (ix = 3; ix < w - 3; ix++) {
			img[3 * (iy * w + ix) + 0] = (int)(255.0 * (px0 + xperpixel * ix));
			img[3 * (iy * w + ix) + 1] = 0;
			img[3 * (iy * w + ix) + 2] = (int)(255.0 * (py1 - yperpixel * iy));
		}


	// want to iterate over mercator space 
	double mpx =px0, mpy = py0, ra, dec;
	double imagex, imagey;
//	imh = 600; imw=800;
	for (j=0; j<h; j++) {
		for (i=0; i<w; i++) {
//			mpx += xperpixel;
//			mpy += yperpixel;
			mpx = px0 + i*(px1-px0)/w;
			mpy = py1 - j*(py1-py0)/h;
			ra = rad2deg(mercx2ra(mpx));
			dec = rad2deg(mercy2dec(mpy));

			tan_radec2pixelxy(&wcs, ra, dec, &imagex, &imagey);
			int pppx = lround(imagex);
			int pppy = lround(imagey);
			if (0 <= pppx && pppx < imw &&
			    0 <= pppy && pppy < imh) {
				// nearest neighbour. bilinear is for weenies.
				img[4 * (j * w + i) + 0] = imbuf[3 * (imw * pppy + pppx) + 0];
				img[4 * (j * w + i) + 1] = imbuf[3 * (imw * pppy + pppx) + 1];
				img[4 * (j * w + i) + 2] = imbuf[3 * (imw * pppy + pppx) + 2];
				img[4 * (j * w + i) + 3] = 255;
				fprintf(stderr, "VVV  HIT pppx=%d, pppy=%d\n", pppx, pppy);
				fprintf(stderr, "VVV     i(x)=%d, j(y)=%d\n", i, j);
				fprintf(stderr, "VVV     ra  =%lf, dec=%lf\n", ra, dec);
				fprintf(stderr, "VVV     mpx =%lf, mpy=%lf\n", mpx,mpy);

		  } else {
				// for now only semitransparent
				img[4 * (j * w + i) + 3] = 55;
		  }
		}
	}

	writeimage(img, w, h);

	return 0;
}
