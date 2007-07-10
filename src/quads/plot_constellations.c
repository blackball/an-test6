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

#include <cairo.h>

#include <png.h>

#include <ppm.h>

#include "sip_qfits.h"
#include "an-bool.h"
#include "qfits.h"
#include "starutil.h"
#include "bl.h"
#include "xylist.h"
#include "rdlist.h"
#include "boilerplate.h"

const char* OPTIONS = "hi:o:w:W:H:";

void print_help(char* progname) {
    boilerplate_help_header(stdout);
    printf("\nUsage: %s\n"
           "   -w <WCS input file>\n"
           "   -o <PNG output file>\n"
           "   (  [-i <PPM input file>]\n"
           "   OR [-W <width> -H <height>] )\n"
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

// fires an ALPHA png out to fout
static void write_png(unsigned char * img, int w, int h, FILE* fout)
{
    png_bytepp image_rows;
    png_structp png_ptr;
    png_infop png_info;
    int n;

    image_rows = malloc(sizeof(png_bytep)*h);
    for (n = 0; n < h; n++)
        image_rows[n] = (unsigned char *) img + 4*n*w;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_info = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, fout);
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

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int c;
    char* wcsfn = NULL;
    char* outfn = NULL;
    char* infn = NULL;
    sip_t sip;
    FILE* fout;

    FILE* fconst = NULL;
    cairo_t* cairo;
    cairo_surface_t* target;
    double lw = 2.0;
    uint32_t nstars;
    size_t mapsize;
    void* map;
    unsigned char* hip;
    FILE* fhip = NULL;
    int W, H;
    unsigned char* img;

    qfits_header* hdr;
    int i;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
        case 'h':
            print_help(args[0]);
            exit(0);
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

    if (infn) {
        int r;
        int R, C, format;
        pixval maxval;
        pixel* pixelrow;
        FILE* fin;

        fin = fopen(infn, "rb");
        if (!fin) {
            fprintf(stderr, "Failed to read input image %s: %s\n", infn, strerror(errno));
            exit(-1);
        }
        ppm_init(&argc, args);
        ppm_readppminit(fin, &C, &R, &maxval, &format);
        pixelrow = ppm_allocrow(C);

        printf("%i x %i, maxval %i, format 0x%x\n", C, R, maxval, format);

        // Allocate image.
        W = C;
        H = R;
        img = malloc(4 * W * H);

        for (r=0; r<R; r++) {
            int c;
            ppm_readppmrow(fin, pixelrow, C, maxval, format);
            for (c=0; c<C; c++) {
                pixel p;
                if (maxval == 255)
                    p = pixelrow[c];
                else
                    PPM_DEPTH(p, pixelrow[c], maxval, 255);
                // Cairo ARGB
                img[4 * (r * C + c) + 0] = 255;
                img[4 * (r * C + c) + 1] = PPM_GETR(p);
                img[4 * (r * C + c) + 2] = PPM_GETG(p);
                img[4 * (r * C + c) + 3] = PPM_GETB(p);
            }
        }
        ppm_freerow(pixelrow);
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

    fprintf(stderr, "render_constellation: Starting.\n");

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

    target = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32, W, H, W*4);
    cairo = cairo_create(target);
    cairo_set_line_width(cairo, lw);
    cairo_set_line_join(cairo, CAIRO_LINE_JOIN_BEVEL);
    cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);
    cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);

    for (c=0;; c++) {
        char shortname[16];
        int nlines;
        int i;
        il* uniqstars = il_new(16);
        double cmass[3];
        double ra,dec;
        double px,py;
        unsigned char r,g,b;

        if (feof(fconst))
            break;

        if (fscanf(fconst, "%s %d ", shortname, &nlines) != 2) {
            fprintf(stderr, "failed parse name+nlines (constellation %i)\n", c);
            fprintf(stderr, "file offset: %i (%x)\n",
                    (int)ftello(fconst), (int)ftello(fconst));
            return -1;
        }
        //fprintf(stderr, "Name: %s.  Nlines %i.\n", shortname, nlines);

        r = (rand() % 128) + 127;
        g = (rand() % 128) + 127;
        b = (rand() % 128) + 127;

        for (i=0; i<nlines; i++) {
            int star1, star2;
            double ra1, dec1, ra2, dec2;
            double px1, px2, py1, py2;

            if (fscanf(fconst, " %d %d", &star1, &star2) != 2) {
                fprintf(stderr, "failed parse star1+star2\n");
                return -1;
            }

            il_insert_unique_ascending(uniqstars, star1);
            il_insert_unique_ascending(uniqstars, star2);

            // RA,DEC are the first two elements: 32-bit floats
            // (little-endian)
            hip_get_radec(hip, star1, &ra1, &dec1);
            hip_get_radec(hip, star2, &ra2, &dec2);

            cairo_set_source_rgba(cairo, r/255.0,g/255.0,b/255.0,0.8);

            //draw_segmented_line(ra1, dec1, ra2, dec2, SEGS, cairo, args);

            if (!sip_radec2pixelxy(&sip, ra1, dec1, &px1, &py1) ||
                !sip_radec2pixelxy(&sip, ra2, dec2, &px2, &py2))
                continue;

            cairo_move_to(cairo, px1, py1);
            cairo_line_to(cairo, px2, py2);

            cairo_stroke(cairo);
        }
        fscanf(fconst, "\n");

        // find center of mass.
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

        if (!sip_radec2pixelxy(&sip, ra, dec, &px, &py))
            continue;

        cairo_select_font_face(cairo, "helvetica", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cairo, 14.0);
        cairo_move_to(cairo, px, py);
        cairo_show_text(cairo, shortname);
    }
    fprintf(stderr, "render_constellations: Read %i constellations.\n", c);

    munmap(map, mapsize);

    fclose(fconst);
    fclose(fhip);

    // Cairo's uint32 ARGB32 format is a little different than what we need,
    // which is uchar R,G,B,A.
    for (i=0; i<(H*W); i++) {
        unsigned char r,g,b,a;
        uint32_t ipix = *((uint32_t*)(img + 4*i));
        a = (ipix >> 24) & 0xff;
        r = (ipix >> 16) & 0xff;
        g = (ipix >>  8) & 0xff;
        b = (ipix      ) & 0xff;
        img[4*i + 0] = r;
        img[4*i + 1] = g;
        img[4*i + 2] = b;
        img[4*i + 3] = a;
    }

    fout = fopen(outfn, "wb");
    if (!fout) {
        fprintf(stderr, "Failed to open output file %s: %s\n", outfn, strerror(errno));
        exit(-1);
    }
    write_png(img, W, H, fout);
    if (fclose(fout)) {
        fprintf(stderr, "Failed to close output file %s: %s\n", outfn, strerror(errno));
        exit(-1);
    }

    cairo_surface_destroy(target);
    cairo_destroy(cairo);

    return 0;
}
