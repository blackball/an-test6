#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include <cairo.h>

#include "tilerender.h"
#include "render_boundary.h"
#include "sip_qfits.h"
#include "cairoutils.h"

char* wcs_dirs[] = {
	"/home/gmaps/ontheweb-data/",
	".",
};

static void logmsg(char* format, ...) {
	va_list args;
	va_start(args, format);
	fprintf(stderr, "render_boundary: ");
	vfprintf(stderr, format, args);
	va_end(args);
}

int render_boundary(unsigned char* img, render_args_t* args) {
	// read wcs.
	sip_t wcs;
	int imw, imh;
	int i;
	cairo_t* cairo;
	cairo_surface_t* target;
	double lw = args->linewidth;
	// the line endpoints.
	double ends[8];
	int SEGS=10;
	qfits_header* wcshead = NULL;

	logmsg("Starting.\n");

	for (i=0; i<sizeof(wcs_dirs)/sizeof(char*); i++) {
		char fn[256];
                char realfn[1024];
		snprintf(fn, sizeof(fn), "%s/%s", wcs_dirs[i], args->wcsfn);
                if (!realpath(fn, realfn)) {
                    logmsg("Failed to get realpath() of wcs file %s: %s\n", fn, strerror(errno));
                    if (getcwd(realfn, sizeof(realfn))) {
                        logmsg("(cwd is %s)\n", realfn);
                    }
                    continue;
                }
		logmsg("Trying wcs file: %s (%s)\n", fn, realfn);
		wcshead = qfits_header_read(fn);
		if (wcshead) {
			logmsg("wcs opened ok\n");
			break;
		} else {
			logmsg("wcs didn't open\n");
		}
	}
	if (!wcshead) {
		logmsg("couldn't open any wcs files\n");
		return -1;
	}

	if (!sip_read_header(wcshead, &wcs)) {
		logmsg("failed to read WCS file.\n");
		return -1;
	}

	imw = qfits_header_getint(wcshead, "IMAGEW", -1);
	imh = qfits_header_getint(wcshead, "IMAGEH", -1);
	if ((imw == -1) || (imh == -1)) {
		logmsg("failed to find IMAGE{W,H} in WCS file.\n");
		return -1;
	}

        qfits_header_destroy(wcshead);

	ends[0] = 0.0;
	ends[1] = 0.0;
	ends[2] = 0.0;
	ends[3] = (double)imh;
	ends[4] = (double)imw;
	ends[5] = (double)imh;
	ends[6] = (double)imw;
	ends[7] = 0.0;

	//logmsg("ra: [%g,%g], dec: [%g,%g]\n", args->ramin, args->ramax, args->decmin, args->decmax);

	target = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32,
												 args->W, args->H, args->W*4);
	cairo = cairo_create(target);
	cairo_set_line_width(cairo, lw);
	cairo_set_line_join(cairo, CAIRO_LINE_JOIN_BEVEL);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);
	cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);

	// Four edges...
	for (i=0; i<4; i++) {
		double* ep1 = ends + i*2;
		double* ep2 = ends + ((i+1)%4)*2;
		double ra1, dec1, ra2, dec2;

		sip_pixelxy2radec(&wcs, ep1[0], ep1[1], &ra1, &dec1);
		sip_pixelxy2radec(&wcs, ep2[0], ep2[1], &ra2, &dec2);

		//logmsg("endpoint %i: ra,dec %g,%g\n", i, ra1, dec1);

		draw_segmented_line(ra1, dec1, ra2, dec2, SEGS, cairo, args);
	}
	cairo_stroke(cairo);

	if (args->dashbox > 0.0) {
		double ra, dec;
		double mx, my;
		double dm = 0.5 * args->dashbox;
		double dashes[] = {5, 5};

		// merc coordinate of field center:
		sip_pixelxy2radec(&wcs, 0.5 * imw, 0.5 * imh, &ra, &dec);
		mx = ra2merc(deg2rad(ra));
		my = dec2merc(deg2rad(dec));

		cairo_set_dash(cairo, dashes, sizeof(dashes)/sizeof(double), 0.0);
		draw_line_merc(mx-dm, my-dm, mx-dm, my+dm, cairo, args);
		draw_line_merc(mx-dm, my+dm, mx+dm, my+dm, cairo, args);
		draw_line_merc(mx+dm, my+dm, mx+dm, my-dm, cairo, args);
		draw_line_merc(mx+dm, my-dm, mx-dm, my-dm, cairo, args);
		cairo_stroke(cairo);

		if (args->zoomright) {
			cairo_set_line_width(cairo, lw/2.0);

			// draw lines from the left edge of the dashed box to the
			// right-hand edge of the image.
			cairo_set_dash(cairo, dashes, 0, 0.0);
			draw_line_merc(mx-dm, my-dm, args->xmercmax, args->ymercmin,
						   cairo, args);
			draw_line_merc(mx-dm, my+dm, args->xmercmax, args->ymercmax,
						   cairo, args);
			cairo_stroke(cairo);
		}
		if (args->zoomdown) {
			cairo_set_line_width(cairo, lw/2.0);
			cairo_set_dash(cairo, dashes, 0, 0.0);
			draw_line_merc(mx-dm, my+dm, args->xmercmin, args->ymercmin,
						   cairo, args);
			draw_line_merc(mx+dm, my+dm, args->xmercmax, args->ymercmin,
						   cairo, args);
			cairo_stroke(cairo);
		}
	}

    cairoutils_argb32_to_rgba(img, args->W, args->H);

	cairo_surface_destroy(target);
	cairo_destroy(cairo);

	return 0;
}
