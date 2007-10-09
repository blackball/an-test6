/*
   This file is part of the Astrometry.net suite.
   Copyright 2007 Dustin Lang.

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
#include <math.h>
#include <stdarg.h>
#include <sys/param.h>

#include "ioutils.h"
#include "sip-utils.h"
#include "tilerender.h"
#include "sip_qfits.h"
#include "cairoutils.h"
#include "keywords.h"
#include "md5.h"

static void
ATTRIB_FORMAT(printf,1,2)
logmsg(char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "load-db: ");
    vfprintf(stderr, format, args);
    va_end(args);
}

const char* OPTIONS = "";

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	int argchar;
    int I;
    sl* wcsfiles;
    int i;

	while ((argchar = getopt (argc, args, OPTIONS)) != -1)
		switch (argchar) {
        }

    wcsfiles = sl_new(16);

    for (i=optind; i<argc; i++) {
        sl_append(wcsfiles, args[i]);
    }

    for (I=0; I<sl_size(wcsfiles); I++) {
        char* imgfn;
        char* wcsfn;
        char* dot;
        bool jpeg, png;
        qfits_header* hdr;
        sip_t wcs;
        sip_t* res;
        double ramin, ramax, decmin, decmax;
        float pixeldensity;
        char* basefn;
        char* fn;
        int W, H;

        wcsfn = sl_get(wcsfiles, I);
        dot = strrchr(wcsfn, '.');
        if (!dot) {
            logmsg("filename %s has no suffix.\n", wcsfn);
            continue;
        }

        jpeg = png = FALSE;
        asprintf(&fn, "%.*s.%s", dot-wcsfn, wcsfn, "jpg");
        if (file_exists(fn)) {
            jpeg = TRUE;
        } else {
            free(fn);
            asprintf(&fn, "%.*s.%s", dot-wcsfn, wcsfn, "jpeg");
            if (file_exists(fn)) {
                jpeg = TRUE;
            } else {
                free(fn);
                asprintf(&fn, "%.*s.%s", dot-wcsfn, wcsfn, "png");
                if (file_exists(fn)) {
                    png = TRUE;
                } else {
                    free(fn);
                }
            }
        }
        imgfn = fn;
        
        if (!(jpeg || png)) {
            logmsg("Image file corresponding to WCS file \"%s\" not found.\n", wcsfn);
            continue;
        }

        hdr = qfits_header_read(wcsfn);
        if (!hdr) {
            logmsg("failed to read WCS header from %s\n", wcsfn);
            continue;
        }
        res = sip_read_header(hdr, &wcs);
        qfits_header_destroy(hdr);
        if (!res) {
            logmsg("failed to parse SIP header from %s\n", wcsfn);
            continue;
        }
        W = (int)wcs.wcstan.imagew;
        H = (int)wcs.wcstan.imageh;

        // find the bounds in RA,Dec of this image.
        // magic 10 = step size in pixels for walking the image boundary.
        get_radec_bounds(&wcs, 10, &ramin, &ramax, &decmin, &decmax);

        logmsg("Reading WCS file %s.\n", wcsfn);
        logmsg("Image file is    %s.\n", imgfn);
        logmsg("Image size: %i x %i, type %s\n", (int)W, (int)H, (jpeg ? "jpeg" : "png"));
        logmsg("RA,Dec range for this image: (%g to %g, %g to %g)\n",
               ramin, ramax, decmin, decmax);

        pixeldensity = 1.0 / square(sip_pixel_scale(&wcs));

        asprintf(&basefn, "%.*s", dot-wcsfn, wcsfn);

		/*
		  char* cpy;
		  char* base;
		  cpy = strdup(basefn);
		  base = strdup(basename(cpy));
		  free(cpy);
		*/
        // insert into readimgdb_image(basefilename,imageformat,ramin,ramax,decmin,decmax,imagew,imageh) values ("img1", "jpeg", 90, 120, -10, 10, 800, 600);
        printf("INSERT INTO tile_image(origfilename, origformat, filename, ramin, ramax, decmin, decmax, imagew, imageh) VALUES ("
               "\"%s\", \"%s\", \"%s\", %g, %g, %g, %g, %i, %i);\n", basefn, (jpeg ? "jpeg" : "png"), basefn, ramin, ramax, decmin, decmax, W, H);

        free(basefn);
    }

    sl_free2(wcsfiles);
	return 0;
}
