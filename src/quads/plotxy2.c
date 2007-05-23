/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <cairo.h>

#include "xylist.h"
#include "boilerplate.h"

#define OPTIONS "hW:H:n:N:r:s:i:e:x:y:w:S:"

static void printHelp(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s [options] > output.pgm\n"
		   "  -i <input-file>   Input file (xylist)\n"
		   "  [-W <width>   ]   Width of output image (default: data-dependent).\n"
		   "  [-H <height>  ]   Height of output image (default: data-dependent).\n"
		   "  [-x <x-offset>]   X offset: position of the bottom-left pixel.\n"
		   "  [-y <y-offset>]   Y offset: position of the bottom-left pixel.\n"
		   "  [-n <first-obj>]  First object to plot (default: 0).\n"
		   "  [-N <num-objs>]   Number of objects to plot (default: all).\n"
		   "  [-r <radius>]     Size of markers to plot (default: 5.0).\n"
		   "  [-w <linewidth>]  Linewidth (default: 1.0).\n"
		   "  [-s <shape>]      Shape of markers (default: c):\n"
		   "                      c = circle\n"
		   "  [-S <scale-factor>]  Scale xylist entries by this value before plotting.\n"
		   "  [-e <extension>]  FITS extension to read (default 0).\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *args[]) {
	int argchar;
	char* progname = args[0];
	char* fname = NULL;
	int W = 0, H = 0;
	int n = 0, N = 0;
	double xoff = 0.0, yoff = 0.0;
	int ext = 0;
	double rad = 5.0;
	double lw = 1.0;
	char shape = 'c';
	xylist* xyls;
	double* xyvals;
	int Nxy;
	int i;
	double scale = 1.0;

	cairo_t* cairo;
	cairo_surface_t* target;

	while ((argchar = getopt(argc, args, OPTIONS)) != -1)
		switch (argchar) {
		case 'S':
			scale = atof(optarg);
			break;
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
		case 'n':
			n = atoi(optarg);
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
		case 'w':
			lw = atof(optarg);
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

	// Scale xylist entries.
	if (scale != 1.0) {
		for (i=0; i<(2*Nxy); i++) {
			xyvals[i] *= scale;
		}
	}

	xylist_close(xyls);

	// if required, scan data for max X,Y
	if (!W) {
		double maxX = 0.0;
		for (i=n; i<Nxy; i++)
			if (xyvals[2*i] > maxX)
				maxX = xyvals[2*i];
		W = ceil(maxX + rad - xoff);
	}
	if (!H) {
		double maxY = 0.0;
		for (i=n; i<Nxy; i++)
			if (xyvals[2*i+1] > maxY)
				maxY = xyvals[2*i+1];
		H = ceil(maxY + rad - yoff);
	}

	fprintf(stderr, "Image size %i x %i.\n", W, H);

	// Allocate image.
	target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
	cairo = cairo_create(target);
	cairo_set_line_width(cairo, lw);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);
	cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);

	// Draw markers.
	for (i=n; i<Nxy; i++) {
		double x = xyvals[2*i] + 0.5 - xoff;
		double y = xyvals[2*i+1] + 0.5 - yoff;
		switch (shape) {
		case 'c':
			cairo_arc(cairo, x, y, rad, 0.0, 2.0*M_PI);
			break;
		case '+':
			cairo_move_to(cairo, x - rad, y);
			cairo_line_to(cairo, x - rad*0.5, y);
			cairo_move_to(cairo, x + rad, y);
			cairo_line_to(cairo, x + rad*0.5, y);
			cairo_move_to(cairo, x, y + rad);
			cairo_line_to(cairo, x, y + rad*0.5);
			cairo_move_to(cairo, x, y - rad);
			cairo_line_to(cairo, x, y - rad*0.5);
		}
		cairo_stroke(cairo);
	}

	free(xyvals);

	// Write pgm img.
	printf("P5 %d %d %d\n", W, H, 255);
	for (i=0; i<H; i++) {
		int j;
		uint32_t* row = (uint32_t*)(cairo_image_surface_get_data(target) + i*cairo_image_surface_get_stride(target));
		for (j=0; j<W; j++) {
			unsigned char r = (row[j] >> 16) & 0xff;
			//printf("%c", r);
			putchar(r);
		}
	}

	cairo_surface_destroy(target);
	cairo_destroy(cairo);

	return 0;
}
