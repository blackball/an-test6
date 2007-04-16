/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

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

#include "boilerplate.h"
#include "bl.h"
#include "permutedsort.h"

#define OPTIONS "hW:H:w:"

static void printHelp(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s [options] <quads>  > output.png\n"
		   "  -W <width>        Width of output image.\n"
		   "  -H <height>       Height of output image.\n"
		   "  [-w <width>]      Width of lines to draw (default: 5).\n"
		   "  <x1> <y1> <x2> <y2> <x3> <y3> <x4> <y4> [...]\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *args[]) {
	int argchar;
	char* progname = args[0];
	int W = 0, H = 0;
	int lw = 5;
	int nquads;
	int i;
	dl* coords;

	cairo_t* cairo;
	cairo_surface_t* target;
	uint32_t* pixels;

	coords = dl_new(16);

	while ((argchar = getopt(argc, args, OPTIONS)) != -1)
		switch (argchar) {
		case 'W':
			W = atoi(optarg);
			break;
		case 'H':
			H = atoi(optarg);
			break;
		case 'w':
			lw = atoi(optarg);
			break;
		case 'h':
		case '?':
		default:
			printHelp(progname);
			exit(-1);
		}

	if ((argc - optind) % 8) {
		printHelp(progname);
		exit(-1);
	}

	if (!(W && H)) {
		printHelp(progname);
		exit(-1);
	}

	for (i=optind; i<argc; i++) {
		double pos = atof(args[i]);
		dl_append(coords, pos);
	}
	nquads = dl_size(coords) / 8;

	target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);

	cairo = cairo_create(target);
	cairo_set_line_width(cairo, lw);
	cairo_set_line_join(cairo, CAIRO_LINE_JOIN_BEVEL);
	//cairo_set_line_join(cairo, CAIRO_LINE_JOIN_ROUND);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);

	cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
	//cairo_set_source_rgba(cairo, 1.0, 1.0, 1.0, 1.0);

	for (i=0; i<nquads; i++) {
		int j;
		double theta[4];
		int perm[4];
		double cx, cy;

		// Make the quad convex so Sam's eyes don't bleed.
		cx = cy = 0.0;
		for (j=0; j<4; j++) {
			cx += dl_get(coords, i*8 + j*2);
			cy += dl_get(coords, i*8 + j*2 + 1);
		}
		cx /= 4.0;
		cy /= 4.0;
		for (j=0; j<4; j++) {
			theta[j] = atan2(dl_get(coords, i*8 + j*2 + 1)-cy,
							 dl_get(coords, i*8 + j*2 + 0)-cx);
			perm[j] = j;
		}
		permuted_sort_set_params(theta, sizeof(double), compare_doubles);
		permuted_sort(perm, 4);

		for (j=0; j<4; j++) {
			if (j==0)
				cairo_move_to(cairo, dl_get(coords, i*8 + perm[j]*2), dl_get(coords, i*8 + perm[j]*2 + 1));
			else
				cairo_line_to(cairo, dl_get(coords, i*8 + perm[j]*2), dl_get(coords, i*8 + perm[j]*2 + 1));
		}
		cairo_close_path(cairo);
		cairo_stroke(cairo);
	}

	pixels = (uint32_t*)cairo_image_surface_get_data(target);

	// PPM
	printf("P6 %d %d %d\n", W, H, 255);
	for (i=0; i<H; i++) {
		int j;
		uint32_t* row = (uint32_t*)((unsigned char*)pixels + i * cairo_image_surface_get_stride(target));
		for (j=0; j<W; j++) {
			unsigned char r,g,b;
			r = (row[j] >> 16) & 0xff;
			g = (row[j] >>  8) & 0xff;
			b = (row[j]      ) & 0xff;
			//printf("%c%c%c", r, g, b);
			putchar(r);
			putchar(g);
			putchar(b);
		}
	}

	cairo_surface_destroy(target);
	cairo_destroy(cairo);

	return 0;
}
