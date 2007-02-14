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

#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#include "xylist.h"
#include "boilerplate.h"

#define OPTIONS "hW:H:N:r:s:i:e:x:y:"

static void printHelp(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s [options] > output.pgm\n"
		   "  -i <input-file>   Input file (xylist)\n"
		   "  [-W <width>   ]   Width of output image (default: data-dependent).\n"
		   "  [-H <height>  ]   Height of output image (default: data-dependent).\n"
		   "  [-x <x-offset>]   X offset: position of the bottom-left pixel.\n"
		   "  [-y <y-offset>]   Y offset: position of the bottom-left pixel.\n"
		   "  [-N <num-objs>]   Number of objects to plot (default: all).\n"
		   "  [-r <radius>]     Size of markers to plot (default: 5).\n"
		   "  [-s <shape>]      Shape of markers (default: c):\n"
		   "                      c = circle\n"
		   "  [-e <extension>]  FITS extension to read (default 0).\n"
		   "\n", progname);
}

static void draw_circle(unsigned char* img, int W, int H,
						double xc, double yc, double rad) {
	int ix, iy;

	for (ix=floor(xc - rad); ix<=ceil(xc + rad); ix++) {
		double dy;
		double dx = ix - xc;
		if (ix < 0 || ix >= W)
			continue;
		dy = sqrt(rad*rad - dx*dx);
		iy = rint(yc + dy);
		if (iy >= 0 && iy < H) {
			img[iy * W + ix] = 255;
		}
		iy = rint(yc - dy);
		if (iy >= 0 && iy < H) {
			img[iy * W + ix] = 255;
		}
	}

	for (iy=floor(yc - rad); iy<=ceil(yc + rad); iy++) {
		double dx;
		double dy = iy - yc;
		if (iy < 0 || iy >= H)
			continue;
		dx = sqrt(rad*rad - dy*dy);
		ix = rint(xc + dx);
		if (ix >= 0 && ix < W) {
			img[iy * W + ix] = 255;
		}
		ix = rint(xc - dx);
		if (ix >= 0 && ix < W) {
			img[iy * W + ix] = 255;
		}
	}
}
		   
extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *args[]) {
	int argchar;
	char* progname = args[0];
	char* fname = NULL;
	int W = 0, H = 0;
	int N = 0;
	double xoff = 0.0, yoff = 0.0;
	int ext = 0;
	double rad = 5.0;
	char shape = 'c';
	unsigned char* img;
	xylist* xyls;
	double* xyvals;
	int Nxy;
	int i;

	while ((argchar = getopt(argc, args, OPTIONS)) != -1)
		switch (argchar) {
		case 'i':
			fname = optarg;
			break;
		case 'x':
			xoff = atof(optarg);
			break;
		case 'y':
			yoff = atof(optarg);
			break;
		case 'W':
			W = atoi(optarg);
			break;
		case 'H':
			H = atoi(optarg);
			break;
		case 'N':
			N = atoi(optarg);
			break;
		case 'e':
			ext = atoi(optarg);
			break;
		case 'r':
			rad = atof(optarg);
			break;
		case 's':
			shape = optarg[0];
			break;
		case 'h':
		case '?':
		default:
			printHelp(progname);
			return (OPT_ERR);
		}

	if (optind != argc) {
		printHelp(progname);
		exit(-1);
	}

	if (!fname) {
		printHelp(progname);
		exit(-1);
	}

	// Open xylist.
	xyls = xylist_open(fname);
	if (!xyls) {
		fprintf(stderr, "Failed to open xylist from file %s.\n", fname);
		exit(-1);
	}

	// Find number of entries in xylist.
	Nxy = xylist_n_entries(xyls, ext);
	if (Nxy == -1) {
		fprintf(stderr, "Failed to read FITS extension %i from file %s.\n", ext, fname);
		exit(-1);
	}

	// If N is specified, apply it as a max.
	if (N && (N < Nxy)) {
		Nxy = N;
	}

	// Read xylist entries.
	xyvals = malloc(2 * Nxy * sizeof(double));
	if (xylist_read_entries(xyls, ext, 0, Nxy, xyvals)) {
		fprintf(stderr, "Failed to read XY values from file %s.\n", fname);
		exit(-1);
	}

	xylist_close(xyls);

	// if required, scan data for max X,Y
	if (!W) {
		double maxX = 0.0;
		for (i=0; i<Nxy; i++)
			if (xyvals[2*i] > maxX)
				maxX = xyvals[2*i];
		W = ceil(maxX + rad - xoff);
	}
	if (!H) {
		double maxY = 0.0;
		for (i=0; i<Nxy; i++)
			if (xyvals[2*i+1] > maxY)
				maxY = xyvals[2*i+1];
		H = ceil(maxY + rad - yoff);
	}

	fprintf(stderr, "Image size %i x %i.\n", W, H);

	// Allocate image.
	img = calloc(W * H, 1);

	// Draw markers.
	for (i=0; i<Nxy; i++) {
		switch (shape) {
		case 'c':
			draw_circle(img, W, H, xyvals[2*i] - xoff, xyvals[2*i+1] - yoff, rad);
			break;
		}
	}

	free(xyvals);

	// Write pgm img.
	printf("P5 %d %d %d\n", W, H, 255);
	if (fwrite(img, 1, W*H, stdout) < W*H) {
		fprintf(stderr, "Error writing: %s\n", strerror(errno));
	}

	free(img);

	return 0;
}
