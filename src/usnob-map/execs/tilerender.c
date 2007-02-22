#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include <png.h>

#include "an-bool.h"
#include "tilerender.h"
#include "starutil.h"
#include "render_image.h"

/**
   This program gets called by "tile.php" in response to a client requesting a map
   tile.  The coordinates of the tile are specified as a range of RA and DEC values.

   The RA coordinates are passed as    -x <lower-RA> -X <upper-RA>
   The DEC coordinates are             -y <lower-DEC> -Y <upper-DEC>
   The width and height in pixels are  -w <width> -h <height>
*/

#define OPTIONS "x:y:X:Y:w:h:l:i:W:"


char* layernames[] = { "image" };
render_func_t renderers[] = { render_image };

static void write_png(unsigned char * img, int w, int h);

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	int gotx, goty, gotX, gotY, gotw, goth;
	double xzoom;
	unsigned char* img;
	render_args_t args;
	pl* layers;
	int i;

	memset(&args, 0, sizeof(render_args_t));
	layers = pl_new(16);
	gotx = goty = gotX = gotY = gotw = goth = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'i':
			args.imagefn = strdup(optarg);
			break;
		case 'W':
			args.wcsfn = strdup(optarg);
			break;
		case 'l':
			pl_append(layers, strdup(optarg));
			break;
		case 'x':
			args.ramin = atof(optarg);
			gotx = TRUE;
			break;
		case 'X':
			args.ramax = atof(optarg);
			goty = TRUE;
			break;
		case 'y':
			args.decmin = atof(optarg);
			gotX = TRUE;
			break;
		case 'Y':
			args.decmax = atof(optarg);
			gotY = TRUE;
			break;
		case 'w':
			args.W = atoi(optarg);
			gotw = TRUE;
			break;
		case 'h':
			args.H = atoi(optarg);
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
		fprintf(stderr, "\n");
		exit(-1);
	}

	if (args.W > 1024 || args.H > 1024) {
		fprintf(stderr, "Width or height too large (limit 1024)\n");
		exit(-1);
	}

	// The Google Maps client treat RA as going from -180 to +180; we prefer to
	// think of it going from 0 to 360.  If the lower-RA value is negative, wrap
	// it around...
	if (args.ramin < 0.0) {
		args.ramin += 360.0;
		args.ramax += 360.0;
	}

	args.xmercmin =  ra2merc(deg2rad(args.ramin));
	args.xmercmax =  ra2merc(deg2rad(args.ramax));
	args.ymercmin = dec2merc(deg2rad(args.decmin));
	args.ymercmax = dec2merc(deg2rad(args.decmax));

	args.xmercperpixel = (double)args.W / (args.xmercmax - args.xmercmin);
	args.ymercperpixel = (double)args.H / (args.ymercmax - args.ymercmin);

	args.xpixelpermerc = 1.0 / args.xmercperpixel;
	args.ypixelpermerc = 1.0 / args.ymercperpixel;

	xzoom = args.xpixelpermerc / 256.0;
	args.zoomlevel = (int)rint(log(xzoom) / log(2.0));

	// Allocate a black image.
	img = calloc(4 * args.W * args.H, 1);

	for (i=0; i<pl_size(layers); i++) {
		int j;
		int NR = sizeof(layernames) / sizeof(char*);
		char* layer = pl_get(layers, i);

		if (i) {
			fprintf(stderr, "FIXME: No compositing\n");
			exit(-1);
		}

		for (j=0; j<NR; j++) {
			if (!strcmp(layer, layernames[j])) {
				if (renderers[j](img, &args)) {
					fprintf(stderr, "Renderer \"%s\" failed.\n", layernames[j]);
				}
				break;
			}
		}
	}

	write_png(img, args.W, args.H);

	free(img);

	return 0;
}

// Converts from RA in radians to Mercator X coordinate in [0, 1].
double ra2merc(double ra) {
	return ra / (2.0 * M_PI);
}
// Converts from Mercator X coordinate [0, 1] to RA in radians.
double merc2ra(double x) {
	return x * (2.0 * M_PI);
}
// Converts from DEC in radians to Mercator Y coordinate in [0, 1].
double dec2merc(double dec) {
	return 0.5 + (asinh(tan(dec)) / (2.0 * M_PI));
}
// Converts from Mercator Y coordinate [0, 1] to DEC in radians.
double merc2dec(double y) {
	return atan(sinh((y - 0.5) * (2.0 * M_PI)));
}

// to RA in degrees
double pixel2ra(double pix, render_args_t* args) {
	double mpx = args->xmercmin + pix * args->xmercperpixel;
	return rad2deg(merc2ra(mpx));
}

// to DEC in degrees
double pixel2dec(double pix, render_args_t* args) {
	double mpy = args->ymercmax - pix * args->ymercperpixel;
	return rad2deg(merc2dec(mpy));
}

// fires an ALPHA png out stdout
static void write_png(unsigned char * img, int w, int h)
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

int in_image(int x, int y, render_args_t* args) {
	return (x >= 0 && x < args->W && y >=0 && y < args->H);
}

uchar* pixel(int x, int y, uchar* img, render_args_t* args) {
	return img + 4*(y*args->W + x);
}
