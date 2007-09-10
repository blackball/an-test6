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

#include "an-bool.h"

#include <cairo.h>
#include <ppm.h>

#include "cairoutils.h"
#include "boilerplate.h"
#include "bl.h"
#include "permutedsort.h"

#define OPTIONS "hW:H:w:I:C:PRo:d:"

static void printHelp(char* progname) {
    int i;
	boilerplate_help_header(stdout);
	printf("\nUsage: %s [options] <quads>  > output.png\n"
           "  [-I <input-image>]  Input image (PPM format) to plot over.\n"
           "  [-P]              Write PPM output instead of PNG.\n"
           "  [-C <color>]      Color to plot in: (default: white)\n",
           progname);
    for (i=0;; i++) {
        char* color = cairoutils_get_color_name(i);
        if (!color) break;
        printf("                       %s\n", color);
    }
    printf("  [-W <width> ]       Width of output image.\n"
		   "  [-H <height>]       Height of output image.\n"
		   "  [-w <width>]      Width of lines to draw (default: 5).\n"
		   "  [-R]:  Read quads from stdin.\n"
		   "  [-o <opacity>]\n"
           "  [-d <dimension of \"quad\">]\n"
		   "  <x1> <y1> <x2> <y2> <x3> <y3> <x4> <y4> [...]\n"
		   "\n");
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
    char* infn = NULL;
    bool pngformat = TRUE;
	bool fromstdin = FALSE;
	bool randomcolor = FALSE;
	float a = 1.0;

    unsigned char* img;
	cairo_t* cairo;
	cairo_surface_t* target;
    float r=1.0, g=1.0, b=1.0;
	int dimquads = 4;

	coords = dl_new(16);

	while ((argchar = getopt(argc, args, OPTIONS)) != -1)
		switch (argchar) {
		case 'd':
			dimquads = atoi(optarg);
			break;
        case 'C':
			if (!strcasecmp(optarg, "random"))
				randomcolor = TRUE;
            else if (cairoutils_parse_color(optarg, &r, &g, &b)) {
                fprintf(stderr, "I didn't understand color \"%s\".\n", optarg);
                exit(-1);
            }
            break;
		case 'o':
			a = atof(optarg);
			break;
		case 'R':
			fromstdin = TRUE;
			break;
        case 'I':
            infn = optarg;
            break;
        case 'P':
            pngformat = FALSE;
            break;
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

	if (!fromstdin && ((argc - optind) % (2*dimquads))) {
		printHelp(progname);
		exit(-1);
	}
	if (!((W && H) || infn)) {
		printHelp(progname);
		exit(-1);
	}
    if (infn && (W || H)) {
        printf("Error: if you specify an input file, you can't give -W or -H (width or height) arguments.\n\n");
        printHelp(progname);
        exit(-1);
    }

	for (i=optind; i<argc; i++) {
		double pos = atof(args[i]);
		dl_append(coords, pos);
	}
	if (fromstdin) {
		for (;;) {
			int j;
			double p;
			if (feof(stdin))
				break;
			for (j=0; j<(2*dimquads); j++) {
				if (fscanf(stdin, " %lg", &p) != 1) {
					fprintf(stderr, "Failed to read a quad from stdin.\n");
					exit(-1);
				}
				dl_append(coords, p);
			}
		}
	}
	nquads = dl_size(coords) / (2*dimquads);

    if (infn) {
        ppm_init(&argc, args);
        img = cairoutils_read_ppm(infn, &W, &H);
        if (!img) {
            fprintf(stderr, "Failed to read input image %s.\n", infn);
            exit(-1);
        }
        cairoutils_rgba_to_argb32(img, W, H);
    } else {
        // Allocate a black image.
        img = calloc(4 * W * H, 1);
    }

    target = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32, W, H, W*4);
	cairo = cairo_create(target);
	cairo_set_line_width(cairo, lw);
	cairo_set_line_join(cairo, CAIRO_LINE_JOIN_BEVEL);
	//cairo_set_line_join(cairo, CAIRO_LINE_JOIN_ROUND);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);

	if (!randomcolor)
		cairo_set_source_rgba(cairo, r, g, b, a);

	for (i=0; i<nquads; i++) {
		int j;
		double theta[dimquads];
		int perm[dimquads];
		double cx, cy;

		// Make the quad convex so Sam's eyes don't bleed.
		cx = cy = 0.0;
		for (j=0; j<dimquads; j++) {
			cx += dl_get(coords, i*(2*dimquads) + j*2);
			cy += dl_get(coords, i*(2*dimquads) + j*2 + 1);
		}
		cx /= dimquads;
		cy /= dimquads;
		for (j=0; j<dimquads; j++) {
			theta[j] = atan2(dl_get(coords, i*(2*dimquads) + j*2 + 1)-cy,
							 dl_get(coords, i*(2*dimquads) + j*2 + 0)-cx);
			perm[j] = j;
		}
		permuted_sort_set_params(theta, sizeof(double), compare_doubles);
		permuted_sort(perm, dimquads);

		if (randomcolor) {
			r = ((rand() % 128) + 127) / 255.0;
			g = ((rand() % 128) + 127) / 255.0;
			b = ((rand() % 128) + 127) / 255.0;
			cairo_set_source_rgba(cairo, r, g, b, a);
		}

		for (j=0; j<dimquads; j++) {
			if (j==0)
				cairo_move_to(cairo,
							  dl_get(coords, i*(2*dimquads) + perm[j]*2),
							  dl_get(coords, i*(2*dimquads) + perm[j]*2 + 1));
			else
				cairo_line_to(cairo,
							  dl_get(coords, i*(2*dimquads) + perm[j]*2),
							  dl_get(coords, i*(2*dimquads) + perm[j]*2 + 1));
		}
		cairo_close_path(cairo);
		cairo_stroke(cairo);
	}

    // Convert image for output...
    cairoutils_argb32_to_rgba(img, W, H);

    if (pngformat) {
        if (cairoutils_stream_png(stdout, img, W, H)) {
            fprintf(stderr, "Failed to write PNG.\n");
            exit(-1);
        }
    } else {
        if (cairoutils_stream_ppm(stdout, img, W, H)) {
            fprintf(stderr, "Failed to write PPM.\n");
            exit(-1);
        }
    }

	cairo_surface_destroy(target);
	cairo_destroy(cairo);
    free(img);

	return 0;
}
