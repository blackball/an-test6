#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#include "starutil.h"

#define OPTIONS "hr:d:W:H:z:s:e:o:g"

static void printHelp(char* progname) {
	fprintf(stderr, "usage: %s\n"
			"    -r <RA>  in degrees\n"
			"    -d <DEC> in degrees\n"
			"    [-W <width>] in pixels, default 256\n"
			"    [-H <height>] in pixels, default 256\n"
			"    [-z <zoom steps>] number of animation frames, default 10, must be >=2\n"
			"    [-s <start-zoom>] default 15\n"
			"    [-e <end-zoom>] default 1\n"
			"    -o <output-file-template> in printf format, given frame number, eg, \"frame%%03i.ppm\".\n"
			"    [-g]  output GIF format\n"
			"\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *args[]) {
    int argchar;
	char* progname = args[0];

	char* outfn = NULL;
	double ra=-1.0, dec=-1.0;
	int W = 256;
	int H = 256;
	int zoomsteps = 10;
	double startzoom = 15.0;
	double endzoom = 15.0;

	double dzoom;
	int i;
	double ucenter, vcenter;
	bool gif = FALSE;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case 'h':
			printHelp(progname);
			exit(0);
		case 'g':
			gif = TRUE;
			break;
		case 'r':
			ra = atof(optarg);
			break;
		case 'd':
			dec = atof(optarg);
			break;
		case 'W':
			W = atoi(optarg);
			break;
		case 'H':
			H = atoi(optarg);
			break;
		case 'z':
			zoomsteps = atoi(optarg);
			break;
		case 's':
			startzoom = atof(optarg);
			break;
		case 'e':
			endzoom = atof(optarg);
			break;
		case 'o':
			outfn = optarg;
			break;
		}

	if (!outfn) {
		printHelp(progname);
		exit(-1);
	}

	if (zoomsteps < 2) {
		printHelp(progname);
		exit(-1);
	}

	dzoom = (endzoom - startzoom) / (zoomsteps - 1);

	ra = deg2rad(ra);
	dec = deg2rad(dec);

	// mercator center.
	ucenter = ra / (2.0 * M_PI);
	while (ucenter < 0.0) ucenter += 1.0;
	while (ucenter > 1.0) ucenter -= 1.0;
	vcenter = asinh(tan(dec));

	for (i=0; i<zoomsteps; i++) {
		double zoom = startzoom + i * dzoom;
		double zoomscale = pow(2.0, 1.0 - zoom);
		char fn[256];
		char cmdline[1024];
		// Google's definition of "scale 1" pixel scale.
		double uscale = 1.0 / 256.0;
		double vscale = (2.0*M_PI) / 256.0;
		double u1, u2, v1, v2;
		double ra1, ra2, dec1, dec2;
		int res;

		uscale *= zoomscale;
		vscale *= zoomscale;

		u1 = ucenter - W/2 * uscale;
		u2 = ucenter + W/2 * uscale;
		v1 = vcenter - H/2 * vscale;
		v2 = vcenter + H/2 * vscale;

		// CHECK BOUNDS!

		ra1 = u1 * 2.0 * M_PI;
		ra2 = u2 * 2.0 * M_PI;
		dec1 = atan(sinh(v1));
		dec2 = atan(sinh(v2));

		ra1 = rad2deg(ra1);
		ra2 = rad2deg(ra2);
		dec1 = rad2deg(dec1);
		dec2 = rad2deg(dec2);

		printf("Zoom %g, scale %g, u range [%g,%g], v range [%g,%g], RA [%g,%g], DEC [%g,%g]\n",
			   zoom, zoomscale, u1, u2, v1, v2, ra1, ra2, dec1, dec2);

		sprintf(fn, outfn, i);

		sprintf(cmdline, "usnobtile -f -x %g, -X %g, -y %g, -Y %g -w %i -h %i %s> %s",
				ra1, ra2, dec1, dec2, W, H, (gif ? "| pnmquant 256 | ppmtogif " : ""), fn);

		printf("cmdline: %s\n", cmdline);

		if ((res = system(cmdline)) == -1) {
			fprintf(stderr, "system() call failed.\n");
			exit(-1);
		}
		if (res) {
			fprintf(stderr, "command line returned a non-zero value.  Quitting.\n");
			break;
		}
	}

	return 0;
}
