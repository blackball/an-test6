#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <sys/param.h>

#include "ioutils.h"
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

static void get_radec_bounds(sip_t* wcs, int W, int H,
                             double* pramin, double* pramax,
                             double* pdecmin, double* pdecmax) {
    double ramin, ramax, decmin, decmax;
    int i, side;
    int STEP = 10;
    // Walk the perimeter of the image in steps of STEP pixels
    // to find the RA,Dec min/max.
    int offsetx[] = { STEP, W, W, 0 };
    int offsety[] = { 0, 0, H, H };
    int stepx[] = { +STEP, 0, -STEP, 0 };
    int stepy[] = { 0, +STEP, 0, -STEP };
    int Nsteps[] = { (W/STEP)-1, H/STEP, W/STEP, H/STEP };
    double lastra, lastdec;

    sip_pixelxy2radec(wcs, 0, 0, &lastra, &lastdec);
    ramin = ramax = lastra;
    decmin = decmax = lastdec;

    for (side=0; side<4; side++) {
        for (i=0; i<Nsteps[side]; i++) {
            double ra, dec;
            int x, y;
            x = offsetx[side] + i * stepx[side];
            y = offsety[side] + i * stepy[side];
            sip_pixelxy2radec(wcs, x, y, &ra, &dec);

            decmin = MIN(decmin, dec);
            decmax = MAX(decmax, dec);

            // Did we just walk over the RA wrap-around line?
            if ((lastra < 90 && ra > 270) ||
                (lastra > 270 && ra < 90)) {
                ramin = 0.0;
                ramax = 360.0;
            } else {
                ramin = MIN(ramin, ra);
                ramax = MAX(ramax, ra);
            }
        }
    }
    if (pramin) *pramin = ramin;
    if (pramax) *pramax = ramax;
    if (pdecmin) *pdecmin = decmin;
    if (pdecmax) *pdecmax = decmax;
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	int argchar;

    int I;
    sl* wcsfiles;
    float* counts;
    float* ink;
    int i, j, w;
    double *ravals, *decvals;

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
        }

    wcsfile = sl_new(16);

    for (i=optind; i<argc; i++) {
        sl_append(wcsfile, args[i]);
    }

    for (I=0; I<sl_size(imagefiles); I++) {
        char* imgfn;
        char* wcsfn;
        char* dot;
        bool jpeg, png;
        qfits_header* hdr;
        sip_t wcs;
        sip_t* res;
        double ra, dec;
        double imagex, imagey;
        double ramin, ramax, decmin, decmax;
        float pixeldensity;
        char fn[1024];

        wcsfn = sl_get(imagefiles, I);
        dot = strrchr(wcsfn, '.');
        if (!dot) {
            logmsg("filename %s has no suffix.\n", wcsfn);
            continue;
        }

        jpeg = png = FALSE;
        snprintf(fn, sizeof(fn), "%.*s.%s", dot-wcsfn, wcsfn, "jpg");
        if (file_exists(fn)) {
            jpeg = TRUE;
        } else {
            snprintf(fn, sizeof(fn), "%.*s.%s", dot-wcsfn, wcsfn, "jpeg");
            if (file_exists(fn)) {
                jpeg = TRUE;
            } else {
                snprintf(fn, sizeof(fn), "%.*s.%s", dot-wcsfn, wcsfn, "png");
                if (file_exists(fn)) {
                    png = TRUE;
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
        W = wcs.wcstan.imagew;
        H = wcs.wcstan.imageh;

        // find the bounds in RA,Dec of this image.
        get_radec_bounds(&wcs, W, H, &ramin, &ramax, &decmin, &decmax);

        logmsg("Reading WCS %s and image %s\n", wcsfn, imgfn);

        logmsg("RA,Dec range for this image: (%g to %g, %g to %g)\n",
               ramin, ramax, decmin, decmax);

        pixeldensity = 1.0 / square(sip_pixel_scale(&wcs));
    }

    sl_free2(wcsfiles);
	return 0;
}
