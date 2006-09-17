#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#include "starutil.h"

#define OPTIONS "hr:d:W:H:z:s:e:o:gn"

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
			"    [-n]  just print the command-lines, don't execute them.\n"
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
	bool justprint = FALSE;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case 'h':
			printHelp(progname);
			exit(0);
		case 'n':
			justprint = TRUE;
			break;
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
		// Google's definition of "scale 0" pixel scale.
		double uscale = 1.0 / 256.0;
		double vscale = (2.0*M_PI) / 256.0;
		double u1, u2, v1, v2;
		double ra1, ra2, dec1, dec2;
		int res;

		uscale *= zoomscale;
		vscale *= zoomscale;

		// CHECK BOUNDS!


		if (zoom <= 5.0) {
			int zi;
			char* map_template = "/h/42/dstn/local/maps/usnob-zoom%i.ppm";
			char fn[256];
			char outfile[256];
			int left, right, top, bottom;
			double scaleadj;
			int pixelsize;
			zi = (int)ceil(zoom);
			if (zi < 1) zi = 1;
			printf("Zoom %g => %i\n", zoom, zi);
			sprintf(fn, map_template, zi);
			scaleadj = pow(2.0, (double)zi - zoom);
			printf("Scale adjustment %g\n", scaleadj);

			pixelsize = (int)rint(pow(2.0, zi)) * 256;

			printf("Pixelsize %i\n", pixelsize);

			// I seem to have gained a factor of two somewhere along the way...
			uscale *= 0.5;
			vscale *= 0.5;

			u1 = ucenter - W/2 * uscale;
			u2 = ucenter + W/2 * uscale;
			v1 = vcenter - H/2 * vscale;
			v2 = vcenter + H/2 * vscale;

			printf("u range [%g, %g], v range [%g, %g].\n", u1, u2, v1, v2);

			left   = (int)rint(u1 * pixelsize);
			right  = (int)rint(u2 * pixelsize);
			top    = (int)rint((v1 + M_PI) / (2.0 * M_PI) * pixelsize);
			bottom = (int)rint((v2 + M_PI) / (2.0 * M_PI) * pixelsize);

			printf("L %i, R %i, T %i, B %i\n", left, right, top, bottom);

			sprintf(outfile, outfn, i);

			if (left >= 0 && right < pixelsize && top >= 0 && bottom < pixelsize) {
				printf("Cutting and scaling...\n");
				sprintf(cmdline, "pnmcut -left %i -right %i -top %i -bottom %i %s | pnmscale -width=%i -height=%i - > %s",
						left, right, top, bottom, fn, W, H, outfile);

				printf("cmdline: %s\n", cmdline);

				if (justprint)
					continue;
				if ((res = system(cmdline)) == -1) {
					fprintf(stderr, "system() call failed.\n");
					exit(-1);
				}
				if (res) {
					fprintf(stderr, "command line returned a non-zero value.  Quitting.\n");
					break;
				}
			} else {
				// we've got to paste together the image...

				sprintf(cmdline, "ppmmake black %i %i > %s", W, H, outfile);
				printf("cmdline: %s\n", cmdline);
				if (!justprint)
					system(cmdline);

				if (left < 0) {
					int L;
					int T, B;
					char* tempimg = "/tmp/tmpimg.ppm";
					int SX, SY;
					T = (top >= 0 ? top : 0);
					B = (bottom < pixelsize ? bottom : pixelsize-1);

					L = left;
					SX = 0;
					while (L < right) {
						int pixL, pixR;
						int SW, SH;
						pixL = ((L % pixelsize) + pixelsize) % pixelsize;
						if (L < 0)
							pixR = pixelsize-1;
						else
							pixR = (right >= pixelsize ? pixelsize-1 : right);
						printf("grabbing %i to %i\n", pixL, pixR);
						SW = rint((1 + pixR - pixL) / scaleadj);
						SH = (1 + B - T) / scaleadj;
						//SX = (L - left) / scaleadj;
						SY = (T - top) / scaleadj;
						if (SX + SW > W) {
							printf("Adjusting width from %i to %i.\n", SW, W-SX);
							SW = W - SX;
						}
						//if (SY + SH > H) {
						sprintf(cmdline, "pnmcut -left %i -right %i -top %i -bottom %i %s "
								"| pnmscale -width=%i -height=%i - "
								"| pnmpaste - %i %i %s > %s; mv %s %s",
								pixL, pixR, T, B, fn,
								SW, SH, SX, SY, outfile, tempimg, tempimg, outfile);
						printf("cmdline: %s\n", cmdline);
						if (!justprint)
							system(cmdline);

						SX += SW;
						L += (pixR - pixL + 1);
					}
				}


			}

		} else {
			u1 = ucenter - W/2 * uscale;
			u2 = ucenter + W/2 * uscale;
			v1 = vcenter - H/2 * vscale;
			v2 = vcenter + H/2 * vscale;

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

			if (justprint)
				continue;
			if ((res = system(cmdline)) == -1) {
				fprintf(stderr, "system() call failed.\n");
				exit(-1);
			}
			if (res) {
				fprintf(stderr, "command line returned a non-zero value.  Quitting.\n");
				break;
			}
		}
	}

	return 0;
}
