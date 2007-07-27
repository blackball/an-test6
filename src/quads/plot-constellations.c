/*
  This file is part of the Astrometry.net suite.
  Copyright 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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
#include <errno.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

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

const char* OPTIONS = "hi:o:w:W:H:s:NCp";

void print_help(char* progname) {
    boilerplate_help_header(stdout);
    printf("\nUsage: %s\n"
           "   -w <WCS input file>\n"
           "   -o <image output file; \"-\" for stdout>\n"
           "   [-p]: write PPM output - default is PNG\n"
           "   (  [-i <PPM input file>]\n"
           "   OR [-W <width> -H <height>] )\n"
           "   [-s <scale>]: scale image coordinates by this value before plotting.\n"
           "   [-N]: plot NGC objects\n"
           "   [-C]: plot constellations\n"
           "\n", progname);
}

static char* const_dirs[] = {
    ".",
    "/usr/share/stellarium/data/sky_cultures/western", // Debian
    "/home/gmaps/usnob-map/execs" // FIXME
};

static char* hipparcos_fn = "hipparcos.fab";
static char* constfn = "constellationship.fab";
static char* hip_dirs[] = {
    ".",
    "/usr/share/stellarium/data", // Debian
    "/home/gmaps/usnob-map/execs"
};

// size of entries in Stellarium's hipparcos.fab file.
static int HIP_SIZE = 15;
// byte offset to the first element in Stellarium's hipparcos.fab file.
static int HIP_OFFSET = 4;

#if __BYTE_ORDER == __BIG_ENDIAN
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif
// Stellarium writes things in little-endian format.
static inline void swap(void* p, int nbytes) {
#if IS_BIG_ENDIAN
    int i;
    unsigned char* c = p;
    for (i=0; i<(nbytes/2); i++) {
        unsigned char tmp = c[i];
        c[i] = c[nbytes-(i+1)];
        c[nbytes-(i+1)] = tmp;
    }
#else
    return;
#endif
}
static inline void swap_32(void* p) {
    return swap(p, 4);
}

typedef union {
    uint32_t i;
    float    f;
} intfloat;

static void hip_get_radec(unsigned char* hip, int star1,
                          double* ra, double* dec) {
    intfloat ifval;
    ifval.i = *((uint32_t*)(hip + HIP_SIZE * star1));
    swap_32(&ifval.i);
    *ra = ifval.f;
    // Stellarium stores RA in hours...
    *ra *= (360.0 / 24.0);
    ifval.i = *((uint32_t*)(hip + HIP_SIZE * star1 + 4));
    swap_32(&ifval.i);
    *dec = ifval.f;
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int c;
    char* wcsfn = NULL;
    char* outfn = NULL;
    char* infn = NULL;
    sip_t sip;
    double scale = 1.0;
    bool pngformat = TRUE;

    FILE* fconst = NULL;
    cairo_t* cairo;
    cairo_surface_t* target;
    double lw = 2.0;
    // circle linewidth.
    double cw = 2.0;

    // leave a gap short of connecting the points.
    double endgap = 5.0;
    // circle radius.
    double crad = endgap;

    double fontsize = 14.0;

    double label_offset = 15.0;

    uint32_t nstars;
    size_t mapsize;
    void* map;
    unsigned char* hip;
    FILE* fhip = NULL;
    int W, H;
    unsigned char* img;

    qfits_header* hdr;
    int i;

    bool NGC = FALSE, constell = FALSE;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
        case 'h':
            print_help(args[0]);
            exit(0);
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

    if (!outfn || !wcsfn) {
        fprintf(stderr, "Need -o and -w args.\n");
        print_help(args[0]);
        exit(-1);
    }
    if (!(infn || (W>0 && H>0))) {
        fprintf(stderr, "Need -i or (-W and -H) args.\n");
        print_help(args[0]);
        exit(-1);
    }

    if (!(NGC || constell)) {
        fprintf(stderr, "Neither constellations nor NGC overlays selected!\n");
        print_help(args[0]);
        exit(-1);
    }

    // adjust for scaling...
    lw /= scale;
    cw /= scale;
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
    } else {
        // Allocate a black image.
        img = calloc(4 * W * H, 1);
    }

    // read WCS.
    hdr = qfits_header_read(wcsfn);
    if (!hdr) {
        fprintf(stderr, "Failed to read FITS header from file %s.\n", wcsfn);
        exit(-1);
    }

    fprintf(stderr, "Trying to parse SIP/TAN header from %s...\n", wcsfn);
    if (sip_read_header(hdr, &sip)) {
        fprintf(stderr, "Got SIP header.\n");
    } else {
        fprintf(stderr, "Failed to parse SIP/TAN header from %s.\n", wcsfn);
        exit(-1);
    }

    srand(0);

    target = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32, W, H, W*4);
    cairo = cairo_create(target);
    cairo_set_line_join(cairo, CAIRO_LINE_JOIN_BEVEL);
    cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);
    cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
    cairo_scale(cairo, scale, scale);
    cairo_select_font_face(cairo, "helvetica", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cairo, fontsize);

    if (constell) {
        for (i=0; i<sizeof(const_dirs)/sizeof(char*); i++) {
            char fn[256];
            snprintf(fn, sizeof(fn), "%s/%s", const_dirs[i], constfn);
            fprintf(stderr, "render_constellation: Trying file: %s\n", fn);
            fconst = fopen(fn, "rb");
            if (fconst)
                break;
        }
        if (!fconst) {
            fprintf(stderr, "render_constellation: couldn't open any constellation files.\n");
            return -1;
        }

        for (i=0; i<sizeof(hip_dirs)/sizeof(char*); i++) {
            char fn[256];
            snprintf(fn, sizeof(fn), "%s/%s", hip_dirs[i], hipparcos_fn);
            fprintf(stderr, "render_constellation: Trying hip file: %s\n", fn);
            fhip = fopen(fn, "rb");
            if (fhip)
                break;
        }
        if (!fhip) {
            fprintf(stderr, "render_constellation: unhip\n");
            return -1;
        }

        // first 32-bit int: 
        if (fread(&nstars, 4, 1, fhip) != 1) {
            fprintf(stderr, "render_constellation: failed to read nstars.\n");
            return -1;
        }
        swap_32(&nstars);
        fprintf(stderr, "render_constellation: Found %i Hipparcos stars\n", nstars);

        mapsize = nstars * HIP_SIZE + HIP_OFFSET;
        map = mmap(0, mapsize, PROT_READ, MAP_SHARED, fileno(fhip), 0);
        hip = ((unsigned char*)map) + HIP_OFFSET;
        //fprintf(stderr, "mapsize: %i\n", mapsize);

        for (c=0;; c++) {
            char shortname[16];
            int nlines;
            int i;
            il* uniqstars = il_new(16);
            double cmass[3];
            double ra,dec;
            double px,py;
            unsigned char r,g,b;
			il* stars;
			int Ninbounds;
			cairo_text_extents_t textents;

            if (feof(fconst))
                break;

            if (fscanf(fconst, "%s %d ", shortname, &nlines) != 2) {
                fprintf(stderr, "failed to parse name+nlines (constellation %i)\n", c);
                fprintf(stderr, "file offset: %i (%x)\n",
                        (int)ftello(fconst), (int)ftello(fconst));
                return -1;
            }
            //fprintf(stderr, "Name: %s.  Nlines %i.\n", shortname, nlines);

            r = (rand() % 128) + 127;
            g = (rand() % 128) + 127;
            b = (rand() % 128) + 127;

            cairo_set_line_width(cairo, lw);

			stars = il_new(16);

            for (i=0; i<nlines; i++) {
                int star1, star2;

                if (fscanf(fconst, " %d %d", &star1, &star2) != 2) {
                    fprintf(stderr, "failed parse star1+star2\n");
                    return -1;
                }

                il_insert_unique_ascending(uniqstars, star1);
                il_insert_unique_ascending(uniqstars, star2);

				il_append(stars, star1);
				il_append(stars, star2);
			}
            fscanf(fconst, "\n");

			// Count the number of unique stars belonging to this contellation
			// that are within the image bounds
			Ninbounds = 0;
            for (i=0; i<il_size(uniqstars); i++) {
                double ra, dec, px, py;
                hip_get_radec(hip, il_get(uniqstars, i), &ra, &dec);
                if (!sip_radec2pixelxy(&sip, ra, dec, &px, &py))
                    continue;
				if (px < 0 || py < 0 || px*scale > W || py*scale > H)
					continue;
				Ninbounds++;
			}

			// Only draw this constellation if at least 2 of its stars
			// are within the image bounds.
			if (Ninbounds < 2) {
				il_free(stars);
				il_free(uniqstars);
				continue;
			}

			cairo_set_source_rgba(cairo, r/255.0,g/255.0,b/255.0,0.8);

			for (i=0; i<il_size(stars)/2; i++) {
                int star1, star2;
                double ra1, dec1, ra2, dec2;
                double px1, px2, py1, py2;
                double dx, dy;
                double dist;
                double gapfrac;

				star1 = il_get(stars, i*2);
				star2 = il_get(stars, i*2+1);

                hip_get_radec(hip, star1, &ra1, &dec1);
                hip_get_radec(hip, star2, &ra2, &dec2);

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
			il_free(stars);

            // Draw circles around each star.
            cairo_set_line_width(cairo, cw);
            for (i=0; i<il_size(uniqstars); i++) {
                double ra, dec, px, py;
                hip_get_radec(hip, il_get(uniqstars, i), &ra, &dec);
                if (!sip_radec2pixelxy(&sip, ra, dec, &px, &py))
                    continue;
                cairo_arc(cairo, px, py, crad, 0.0, 2.0*M_PI);
                cairo_stroke(cairo);
            }

            // find center of mass and draw the label there.
            cmass[0] = cmass[1] = cmass[2] = 0.0;
            for (i=0; i<il_size(uniqstars); i++) {
                double xyz[3];
                hip_get_radec(hip, il_get(uniqstars, i), &ra, &dec);
                radecdeg2xyzarr(ra, dec, xyz);
                cmass[0] += xyz[0];
                cmass[1] += xyz[1];
                cmass[2] += xyz[2];
            }
            cmass[0] /= il_size(uniqstars);
            cmass[1] /= il_size(uniqstars);
            cmass[2] /= il_size(uniqstars);
            xyzarr2radecdeg(cmass, &ra, &dec);

			il_free(uniqstars);

            if (!sip_radec2pixelxy(&sip, ra, dec, &px, &py))
                continue;

			fprintf(stderr, "%s at (%g, %g)\n", shortname, px, py);

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

            cairo_move_to(cairo, px, py);
            cairo_show_text(cairo, shortname);
        }
        fprintf(stderr, "render_constellations: Read %i constellations.\n", c);

        munmap(map, mapsize);

        fclose(fconst);
        fclose(fhip);
    }

    if (NGC) {
        int i, N;
        double imscale;
        double imsize;
        double dy;
        cairo_font_extents_t extents;

        //il *ngcids, *icids;

        cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);

        cairo_font_extents(cairo, &extents);
        dy = extents.ascent * 0.5;

        // Code stolen from wcs-annotate.c
        // arcsec/pixel
        imscale = sip_pixel_scale(&sip);
        // arcmin
        imsize = imscale * (imin(W, H) / scale) / 60.0;
        N = ngc_num_entries();

        //ngcids = il_new(16);
        //icids = il_new(16);
        for (i=0; i<N; i++) {
            ngc_entry* ngc = ngc_get_entry(i);
            double px, py;
            char str[256];
            char* buf;
            int len;
            int nwritten;
            pl* names;
            double pixsize;
            float ara, adec;

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
            len = sizeof(str);
            buf = str;
            nwritten = snprintf(buf, len, "%s %i", (ngc->is_ngc ? "NGC" : "IC"), ngc->id);
            buf += nwritten;
            len -= nwritten;

            names = ngc_get_names(ngc);
            if (names) {
                int n;
                for (n=0; n<pl_size(names); n++) {
                    nwritten = snprintf(buf, len, " / %s", (char*)pl_get(names, n));
                    buf += nwritten;
                    len -= nwritten;
                }
            }
            pl_free(names);

            cairo_move_to(cairo, px + label_offset, py + dy);
            cairo_show_text(cairo, str);

            pixsize = ngc->size * 60.0 / imscale;
            cairo_move_to(cairo, px + pixsize/2.0, py);
            cairo_arc(cairo, px, py, pixsize/2.0, 0.0, 2.0*M_PI);
            //fprintf(stderr, "size: %f arcsec, pixsize: %f pixels\n", ngc->size, pixsize);

            cairo_stroke(cairo);
        }
    }

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
