#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#include "starutil.h"
#include "pnmutils.h"

#define OPTIONS "hr:d:W:H:z:s:e:o:gnipw"

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
			"    [-i]  force rendering from pre-rendered images\n"
			"    [-p]  force direct plotting from catalog\n"
			"    [-w]  do automatic white-balancing\n"
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
	bool forceimg = FALSE;
	bool forcecat = FALSE;
	bool whitebalance = FALSE;
	bool whitebalframe = FALSE;
	bool donewhitebalframe = FALSE;
	char* lastfn = NULL;
	double Rgain, Ggain, Bgain;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case 'w':
			whitebalance = TRUE;
			break;
		case 'i':
			forceimg = TRUE;
			break;
		case 'p':
			forcecat = TRUE;
			break;
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

	if (zoomsteps < 1) {
		printHelp(progname);
		exit(-1);
	}

	if (zoomsteps > 1)
		dzoom = (endzoom - startzoom) / (zoomsteps - 1);
	else
		dzoom = 0.0;

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
		// My "scale 1" image is 512x512.
		double uscale = 1.0 / 512.0;
		double vscale = (2.0*M_PI) / 512.0;
		double u1, u2, v1, v2;
		double ra1, ra2, dec1, dec2;
		int res;

		uscale *= zoomscale;
		vscale *= zoomscale;

		// CHECK BOUNDS!

		u1 = ucenter - W/2 * uscale;
		u2 = ucenter + W/2 * uscale;
		v1 = vcenter - H/2 * vscale;
		v2 = vcenter + H/2 * vscale;

		//if ((zoom <= 5.0) || forceimg) {
		if ((!forcecat && (zoom <= 4.0)) || forceimg || whitebalframe) {
			int zi;
			char* map_template = "/h/42/dstn/local/maps/usnob-zoom%i.ppm";
			char fn[256];
			char outfile[256];
			int left, right, top, bottom;
			double scaleadj;
			int pixelsize;
			char* tempimg = "/tmp/tmpimg.ppm";
			/*
			  zi = (int)ceil(zoom);
			  if (zi < 1) zi = 1;
			  if (zi > 5) zi = 5;
			*/

			if (whitebalance && !whitebalframe && !donewhitebalframe) {
				// re-render the previous zoom step from images rather than from catalog.
				whitebalframe = TRUE;
				i-=2;
				continue;
			}

			zi = 5;
			printf("Zoom %g => %i\n", zoom, zi);
			sprintf(fn, map_template, zi);
			scaleadj = pow(2.0, (double)zi - zoom);
			printf("Scale adjustment %g\n", scaleadj);

			pixelsize = (int)rint(pow(2.0, zi)) * 256;

			printf("Pixelsize %i\n", pixelsize);

			printf("u range [%g, %g], v range [%g, %g].\n", u1, u2, v1, v2);

			left   = (int)rint(u1 * pixelsize);
			right  = (int)rint(u2 * pixelsize);
			top    = (int)rint((v1 + M_PI) / (2.0 * M_PI) * pixelsize);
			bottom = (int)rint((v2 + M_PI) / (2.0 * M_PI) * pixelsize);

			printf("L %i, R %i, T %i, B %i\n", left, right, top, bottom);

			if (whitebalframe) {
				sprintf(outfile, "/tmp/whitebal.ppm");
			} else {
				sprintf(outfile, outfn, i);
			}

			if (left >= 0 && right < pixelsize && top >= 0 && bottom < pixelsize) {
				printf("Cutting and scaling...\n");
				if (whitebalance && !whitebalframe) {
					sprintf(cmdline, "pnmcut -left %i -right %i -top %i -bottom %i %s "
							"| pnmscale -width=%i -height=%i - "
							"| pnmflip -tb > %s; "
							"ppmnormrgb -r %g -g %g -b %g %s > %s",
							left, right, top, bottom, fn,
							W, H, tempimg,
							Rgain, Ggain, Bgain, tempimg, outfile);
				} else {
					sprintf(cmdline, "pnmcut -left %i -right %i -top %i -bottom %i %s "
							"| pnmscale -width=%i -height=%i - "
							"| pnmflip -tb > %s",
							left, right, top, bottom, fn, W, H, outfile);
				}

				printf("cmdline: %s\n", cmdline);

				if (whitebalframe) {
					if (pnmutils_whitebalance(lastfn, outfile, &Rgain, &Ggain, &Bgain)) {
						exit(-1);
					}
					printf("Gains: R %g, G %g, B %g\n", Rgain, Ggain, Bgain);
					donewhitebalframe = TRUE;
					whitebalframe = FALSE;
				}

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
						if (SY + SH > H) {
							printf("Adjusting height from %i to %i.\n", SH, H-SY);
							SH = H - SY;
						}
						sprintf(cmdline, "pnmcut -left %i -right %i -top %i -bottom %i %s "
								"| pnmscale -width=%i -height=%i - "
								"| pnmflip -tb "
								"| pnmpaste - %i %i %s > %s; "
								"mv %s %s",
								pixL, pixR, T, B, fn,
								SW, SH,
								SX, SY, outfile, tempimg,
								tempimg, outfile);
						printf("cmdline: %s\n", cmdline);
						if (!justprint)
							system(cmdline);

						SX += SW;
						L += (pixR - pixL + 1);
					}

				}

				if (whitebalance && !whitebalframe) {
					sprintf(cmdline, "ppmnormrgb -r %g -g %g -b %g %s > %s; "
							"mv %s %s",
							Rgain, Ggain, Bgain, outfile, tempimg,
							tempimg, outfile);
					printf("cmdline: %s\n", cmdline);
					if (!justprint)
						system(cmdline);
				}

				if (whitebalframe) {
					if (pnmutils_whitebalance(lastfn, outfile, &Rgain, &Ggain, &Bgain)) {
						exit(-1);
					}
					printf("Gains: R %g, G %g, B %g\n", Rgain, Ggain, Bgain);
					donewhitebalframe = TRUE;
					whitebalframe = FALSE;
				}
			}

		} else {
			ra1 = u1 * 2.0 * M_PI;
			ra2 = u2 * 2.0 * M_PI;
			dec1 = atan(sinh(v1));
			dec2 = atan(sinh(v2));

			ra1 = rad2deg(ra1);
			ra2 = rad2deg(ra2);
			dec1 = rad2deg(dec1);
			dec2 = rad2deg(dec2);

			printf("Zoom %g, scale %g, u range [%g,%g], v range [%g,%g], RA [%g,%g], DEC [%g,%g] degrees\n",
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

			free(lastfn);
			lastfn = strdup(fn);
		}
	}

	free(lastfn);
	return 0;
}
