/*
  This file is part of the Astrometry.net suite.
  Copyright 2007-2008 Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#include "an-bool.h"

#include <cairo.h>
#include <ppm.h>

#include "sip_qfits.h"
#include "qfits.h"
#include "starutil.h"
#include "bl.h"
#include "xylist.h"
#include "rdlist.h"
#include "boilerplate.h"
#include "mathutil.h"
#include "cairoutils.h"
#include "ngc2000.h"
#include "ngcic-accurate.h"
#include "constellations.h"
#include "brightstars.h"

const char* OPTIONS = "hi:o:w:W:H:s:NCBpb:cjvLn:f:M";

void print_help(char* progname) {
    boilerplate_help_header(stdout);
    printf("\nUsage: %s\n"
           "   -w <WCS input file>\n"
		   "   ( -L: just list the items in the field\n"
           " OR  -o <image output file; \"-\" for stdout>  )\n"
           "   [-p]: write PPM output - default is PNG\n"
           "   (  [-i <PPM input file>]\n"
           "   OR [-W <width> -H <height>] )\n"
           "   [-s <scale>]: scale image coordinates by this value before plotting.\n"
           "   [-N]: plot NGC objects\n"
           "   [-C]: plot constellations\n"
		   "   [-B]: plot named bright stars\n"
		   "   [-b <number-of-bright-stars>]: just plot the <N> brightest stars\n"
		   "   [-c]: only plot bright stars that have common names.\n"
		   "   [-j]: if a bright star has a common name, only print that\n"
           "   [-v]: be verbose\n"
           "   [-n <width>]: NGC circle width (default 2)\n"
           "   [-f <size>]: font size.\n"
           "   [-M]: show only NGC/IC and Messier numbers (no common names)\n"
           "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

static int sort_by_mag(const void* v1, const void* v2) {
	const brightstar_t* s1 = v1;
	const brightstar_t* s2 = v2;
	if (s1->Vmag > s2->Vmag)
		return 1;
	if (s1->Vmag == s2->Vmag)
		return 0;
	return -1;
}

static void add_text(cairo_t* cairo, const char* txt, double px, double py) {
    int dx, dy;
    cairo_save(cairo);
    cairo_set_source_rgba(cairo, 0, 0, 0, 1);
    for (dy=-1; dy<=1; dy++) {
        for (dx=-1; dx<=1; dx++) {
            cairo_move_to(cairo, px+dx, py+dy);
            cairo_show_text(cairo, txt);
        }
    }
    cairo_stroke(cairo);
    cairo_restore(cairo);
    cairo_move_to(cairo, px, py);
    cairo_show_text(cairo, txt);
    cairo_stroke(cairo);
}

int main(int argc, char** args) {
    int c;
    char* wcsfn = NULL;
    char* outfn = NULL;
    char* infn = NULL;
    sip_t sip;
    double scale = 1.0;
    bool pngformat = TRUE;

    cairo_t* cairo = NULL;
    cairo_surface_t* target = NULL;
    double lw = 2.0;
    // circle linewidth.
    double cw = 2.0;

    // NGC linewidth
    double nw = 2.0;

    // leave a gap short of connecting the points.
    double endgap = 5.0;
    // circle radius.
    double crad = endgap;

    double fontsize = 14.0;

    double label_offset = 15.0;

    int W = 0, H = 0;
    unsigned char* img = NULL;

    bool NGC = FALSE, constell = FALSE;
	bool bright = FALSE;
	bool common_only = FALSE;
	bool print_common_only = FALSE;
	int Nbright = 0;
    bool verbose = FALSE;
	double ra, dec, px, py;
	int i, N;
	bool justlist = FALSE;
    bool only_messier = FALSE;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
        case 'h':
            print_help(args[0]);
            exit(0);
        case 'M':
            only_messier = TRUE;
            break;
        case 'n':
            nw = atof(optarg);
            break;
        case 'f':
            fontsize = atof(optarg);
            break;
		case 'L':
			justlist = TRUE;
			outfn = NULL;
			break;
        case 'v':
            verbose = TRUE;
            break;
		case 'j':
			print_common_only = TRUE;
			break;
		case 'c':
			common_only = TRUE;
			break;
		case 'b':
			Nbright = atoi(optarg);
			break;
		case 'B':
			bright = TRUE;
			break;
        case 'N':
            NGC = TRUE;
            break;
        case 'C':
            constell = TRUE;
            break;
        case 'p':
            pngformat = FALSE;
            break;
        case 's':
            scale = atof(optarg);
            break;
        case 'o':
            outfn = optarg;
            break;
        case 'i':
            infn = optarg;
            break;
        case 'w':
            wcsfn = optarg;
            break;
        case 'W':
            W = atoi(optarg);
            break;
        case 'H':
            H = atoi(optarg);
            break;
        }
    }

    if (optind != argc) {
        print_help(args[0]);
        exit(-1);
    }

    if (!(outfn || justlist) || !wcsfn) {
        fprintf(stderr, "Need (-o or -L) and -w args.\n");
        print_help(args[0]);
        exit(-1);
    }
	/*
	  if (!(infn || (W>0 && H>0))) {
	  fprintf(stderr, "Need -i or (-W and -H) args.\n");
	  print_help(args[0]);
	  exit(-1);
	  }
	*/

    if (!(NGC || constell || bright)) {
        fprintf(stderr, "Neither constellations, bright stars, nor NGC overlays selected!\n");
        print_help(args[0]);
        exit(-1);
    }

    // adjust for scaling...
    lw /= scale;
    cw /= scale;
    nw /= scale;
    crad /= scale;
    endgap /= scale;
    fontsize /= scale;
    label_offset /= scale;

    if (infn) {
        ppm_init(&argc, args);
        img = cairoutils_read_ppm(infn, &W, &H);
        if (!img) {
            fprintf(stderr, "Failed to read input image %s.\n", infn);
            exit(-1);
        }
        cairoutils_rgba_to_argb32(img, W, H);
    } else if (!justlist) {
        // Allocate a black image.
        img = calloc(4 * W * H, 1);
    }

    // read WCS.
    if (verbose)
        fprintf(stderr, "Trying to parse SIP/TAN header from %s...\n", wcsfn);
    if (sip_read_header_file(wcsfn, &sip)) {
        if (verbose)
            fprintf(stderr, "Got SIP header.\n");
    } else {
        fprintf(stderr, "Failed to parse SIP/TAN header from %s.\n", wcsfn);
        exit(-1);
    }

	if (!W || !H) {
		W = sip.wcstan.imagew;
		H = sip.wcstan.imageh;
	}
	if (!W || !H) {
		fprintf(stderr, "Image W,H unknown.\n");
		exit(-1);
	}

    srand(0);

	if (!justlist) {
		target = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32, W, H, W*4);
		cairo = cairo_create(target);
		cairo_set_line_join(cairo, CAIRO_LINE_JOIN_BEVEL);
		cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);
		cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
		cairo_scale(cairo, scale, scale);
		//cairo_select_font_face(cairo, "helvetica", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
		cairo_select_font_face(cairo, "DejaVu Sans Mono Book", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cairo, fontsize);
	}

    if (constell) {
		N = constellations_n();

		if (verbose)
            fprintf(stderr, "Checking %i constellations.\n", N);
		for (c=0; c<N; c++) {
			const char* shortname;
			const char* longname;
			il* lines;
            il* uniqstars;
			il* inboundstars;
            unsigned char r,g,b;
			int Ninbounds;
			int Nunique;
			cairo_text_extents_t textents;
            double cmass[3];

			uniqstars = constellations_get_unique_stars(c);
			inboundstars = il_new(16);

			Nunique = il_size(uniqstars);
			//fprintf(stderr, "%s: %i unique stars.\n", shortname, il_size(uniqstars));

			// Count the number of unique stars belonging to this contellation
			// that are within the image bounds
			Ninbounds = 0;
            for (i=0; i<il_size(uniqstars); i++) {
				int star;
				star = il_get(uniqstars, i);
				constellations_get_star_radec(star, &ra, &dec);
				//fprintf(stderr, "star %i: ra,dec (%g,%g)\n", il_get(uniqstars, i), ra, dec);
                if (!sip_radec2pixelxy(&sip, ra, dec, &px, &py))
                    continue;
				if (px < 0 || py < 0 || px*scale > W || py*scale > H)
					continue;
				Ninbounds++;
				il_append(inboundstars, star);
			}
			il_free(uniqstars);
			//fprintf(stderr, "%i are in-bounds.\n", Ninbounds);
			// Only draw this constellation if at least 2 of its stars
			// are within the image bounds.
			if (Ninbounds < 2) {
				il_free(inboundstars);
				continue;
			}

			if (!justlist) {
				r = (rand() % 128) + 127;
				g = (rand() % 128) + 127;
				b = (rand() % 128) + 127;
				cairo_set_source_rgba(cairo, r/255.0,g/255.0,b/255.0,0.8);
				cairo_set_line_width(cairo, cw);
			}

			// Draw circles around each star.
            // Also find center of mass (of the in-bounds stars)
            cmass[0] = cmass[1] = cmass[2] = 0.0;
            for (i=0; i<il_size(inboundstars); i++) {
                double xyz[3];
				int star = il_get(inboundstars, i);
				constellations_get_star_radec(star, &ra, &dec);
                if (!sip_radec2pixelxy(&sip, ra, dec, &px, &py))
                    continue;
				if (px < 0 || py < 0 || px*scale > W || py*scale > H)
					continue;
				if (!justlist) {
					cairo_arc(cairo, px, py, crad, 0.0, 2.0*M_PI);
					cairo_stroke(cairo);
				}
                radecdeg2xyzarr(ra, dec, xyz);
                cmass[0] += xyz[0];
                cmass[1] += xyz[1];
                cmass[2] += xyz[2];
            }
            cmass[0] /= il_size(inboundstars);
            cmass[1] /= il_size(inboundstars);
            cmass[2] /= il_size(inboundstars);
            xyzarr2radecdeg(cmass, &ra, &dec);

			il_free(inboundstars);

            if (!sip_radec2pixelxy(&sip, ra, dec, &px, &py))
                continue;

			shortname = constellations_get_shortname(c);
			longname = constellations_get_longname(c);
			assert(shortname && longname);

			if (verbose) fprintf(stderr, "%s at (%g, %g)\n", longname, px, py);

			if (Ninbounds == Nunique) {
				printf("The constellation %s (%s)\n", longname, shortname);
			} else {
				printf("Part of the constellation %s (%s)\n", longname, shortname);
			}

			if (justlist)
				continue;

			// If the label will be off-screen, move it back on.
			cairo_text_extents(cairo, shortname, &textents);
			if (px < 0)
				px = 0;
			if (py < textents.height)
				py = textents.height;
			if ((px + textents.width)*scale > W)
				px = W/scale - textents.width;
			if ((py+textents.height)*scale > H)
				py = H/scale - textents.height;
			//fprintf(stderr, "%s at (%g, %g)\n", shortname, px, py);

            add_text(cairo, longname, px, py);

			// Draw the lines.
            cairo_set_line_width(cairo, lw);
			lines = constellations_get_lines(c);
			for (i=0; i<il_size(lines)/2; i++) {
                int star1, star2;
                double ra1, dec1, ra2, dec2;
                double px1, px2, py1, py2;
                double dx, dy;
                double dist;
                double gapfrac;
				star1 = il_get(lines, i*2+0);
				star2 = il_get(lines, i*2+1);
				constellations_get_star_radec(star1, &ra1, &dec1);
				constellations_get_star_radec(star2, &ra2, &dec2);
                if (!sip_radec2pixelxy(&sip, ra1, dec1, &px1, &py1) ||
                    !sip_radec2pixelxy(&sip, ra2, dec2, &px2, &py2))
                    continue;
                dx = px2 - px1;
                dy = py2 - py1;
                dist = hypot(dx, dy);
                gapfrac = endgap / dist;
                cairo_move_to(cairo, px1 + dx*gapfrac, py1 + dy*gapfrac);
                cairo_line_to(cairo, px1 + dx*(1.0-gapfrac), py1 + dy*(1.0-gapfrac));
                cairo_stroke(cairo);
			}
			il_free(lines);
        }
        if (verbose) fprintf(stderr, "done constellations.\n");
    }

    if (NGC) {
        double imscale;
        double imsize;
        double dy = 0;
        cairo_font_extents_t extents;

		if (!justlist) {
			cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
			cairo_set_line_width(cairo, nw);
			cairo_font_extents(cairo, &extents);
			dy = extents.ascent * 0.5;
		}

        // Code stolen from wcs-annotate.c
        // arcsec/pixel
        imscale = sip_pixel_scale(&sip);
        // arcmin
        imsize = imscale * (imin(W, H) / scale) / 60.0;
        N = ngc_num_entries();

		if (verbose) fprintf(stderr, "Checking %i NGC/IC objects.\n", N);

        for (i=0; i<N; i++) {
            ngc_entry* ngc = ngc_get_entry(i);
			sl* str;
            sl* names;
            double pixsize;
            float ara, adec;
			char* text;

            if (!ngc)
                break;
            if (ngc->size < imsize * 0.02)
                continue;

            if (ngcic_accurate_get_radec(ngc->is_ngc, ngc->id, &ara, &adec) == 0) {
                ngc->ra = ara;
                ngc->dec = adec;
            }

            if (!sip_radec2pixelxy(&sip, ngc->ra, ngc->dec, &px, &py))
                continue;
            if (px < 0 || py < 0 || px*scale > W || py*scale > H)
                continue;

			str = sl_new(4);
			sl_appendf(str, "%s %i", (ngc->is_ngc ? "NGC" : "IC"), ngc->id);

            names = ngc_get_names(ngc);
            if (names) {
                int n;
                for (n=0; n<sl_size(names); n++) {
                    //printf("  >>%s<<\n", sl_get(names, n));
                    if (only_messier && strncmp(sl_get(names, n), "M ", 2))
                        continue;
					sl_appendf(str, " / %s", sl_get(names, n));
                }
            }
            sl_free2(names);
			text = sl_implode(str, "");

			printf("%s\n", text);

			if (!justlist) {
				pixsize = ngc->size * 60.0 / imscale;
				cairo_move_to(cairo, px + pixsize/2.0, py);
				cairo_arc(cairo, px, py, pixsize/2.0, 0.0, 2.0*M_PI);
				//fprintf(stderr, "size: %f arcsec, pixsize: %f pixels\n", ngc->size, pixsize);
				cairo_stroke(cairo);

                add_text(cairo, text, px + label_offset, py + dy);
			}
			free(text);
			sl_free2(str);
        }
    }

	if (bright) {
        double dy = 0;
        cairo_font_extents_t extents;
		pl* brightstars = pl_new(16);

		if (!justlist) {
			cairo_set_source_rgba(cairo, 0.75, 0.75, 0.75, 0.8);
			cairo_font_extents(cairo, &extents);
			dy = extents.ascent * 0.5;
			cairo_set_line_width(cairo, cw);
		}

		N = bright_stars_n();
		if (verbose) fprintf(stderr, "Checking %i bright stars.\n", N);

		for (i=0; i<N; i++) {
			const brightstar_t* bs = bright_stars_get(i);

            if (!sip_radec2pixelxy(&sip, bs->ra, bs->dec, &px, &py))
                continue;
            if (px < 0 || py < 0 || px*scale > W || py*scale > H)
                continue;
			if (!(bs->name && strlen(bs->name)))
				continue;
			if (common_only && !(bs->common_name && strlen(bs->common_name)))
				continue;

			pl_append(brightstars, bs);
		}

		if (Nbright && (pl_size(brightstars) > Nbright)) {
			pl_sort(brightstars, sort_by_mag);
			pl_remove_index_range(brightstars, Nbright, pl_size(brightstars)-Nbright);
		}

		for (i=0; i<pl_size(brightstars); i++) {
			char* text;
			const brightstar_t* bs = pl_get(brightstars, i);

            if (!sip_radec2pixelxy(&sip, bs->ra, bs->dec, &px, &py))
                continue;
			if (bs->common_name && strlen(bs->common_name))
				if (print_common_only || common_only)
					text = strdup(bs->common_name);
				else
					asprintf(&text, "%s (%s)", bs->common_name, bs->name);
			else
				text = strdup(bs->name);

			//printf("Bright star %i/%i: %s, radec (%g,%g), pixel (%g,%g)\n", i, N, text, bs->ra, bs->dec, px, py);
			if (verbose) fprintf(stderr, "%s at (%g, %g)\n", text, px + label_offset, py + dy);

			if (bs->common_name && strlen(bs->common_name))
				printf("The star %s (%s)\n", bs->common_name, bs->name);
			else
				printf("The star %s\n", bs->name);

			if (!justlist)
                add_text(cairo, text, px + label_offset, py + dy);

			free(text);

			if (!justlist) {
				cairo_arc(cairo, px, py, crad, 0.0, 2.0*M_PI);
				cairo_stroke(cairo);
			}
		}
	}

	if (justlist)
		return 0;

    // Convert image for output...
    cairoutils_argb32_to_rgba(img, W, H);

    if (pngformat) {
        if (cairoutils_write_png(outfn, img, W, H)) {
            fprintf(stderr, "Failed to write PNG.\n");
            exit(-1);
        }
    } else {
        if (cairoutils_write_ppm(outfn, img, W, H)) {
            fprintf(stderr, "Failed to write PPM.\n");
            exit(-1);
        }
    }

    cairo_surface_destroy(target);
    cairo_destroy(cairo);
    free(img);

    return 0;
}
