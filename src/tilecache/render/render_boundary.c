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
#include "ioutils.h"

char* wcs_dirs[] = {
	"/home/gmaps/ontheweb-data/",
	"/home/gmaps/apod-solves",
};

static void logmsg(char* format, ...) {
	va_list args;
	va_start(args, format);
	fprintf(stderr, "render_boundary: ");
	vfprintf(stderr, format, args);
	va_end(args);
}

int render_boundary(unsigned char* img, render_args_t* args) {
	int i, I;
	cairo_t* cairo;
	cairo_surface_t* target;
	double lw = args->linewidth;
	sl* wcsfiles = NULL;
	qfits_header* hdr = NULL;
	bool fullfilename = TRUE;

	logmsg("Starting.\n");

    if (args->filelist) {
		fullfilename = FALSE;
        wcsfiles = file_get_lines(args->filelist, FALSE);
        if (!wcsfiles) {
            logmsg("failed to read filelist \"%s\".\n", args->filelist);
            return -1;
        }
        logmsg("read %i filenames from the file \"%s\".\n", sl_size(wcsfiles), args->filelist);
	} else if (args->imwcsfn) {
		wcsfiles = sl_new(4);
		fullfilename = TRUE;
		sl_appendf(wcsfiles, args->imwcsfn);
	}
	if (!sl_size(wcsfiles)) {
		logmsg("No WCS files specified.\n");
		return -1;
	}

	target = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32,
												 args->W, args->H, args->W*4);
	cairo = cairo_create(target);
	cairo_set_line_width(cairo, lw);
	cairo_set_line_join(cairo, CAIRO_LINE_JOIN_ROUND);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);
	cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);

    for (I=0; I<sl_size(wcsfiles); I++) {
		char* basefn;
        char* wcsfn;
        sip_t* res;
		sip_t wcs;
		int W, H;

        basefn = sl_get(wcsfiles, I);
		if (!strlen(basefn)) {
            logmsg("empty filename.\n");
            continue;
		}
		logmsg("Base filename: \"%s\"\n", basefn);

		for (i=0; i<sizeof(wcs_dirs)/sizeof(char*); i++) {
			asprintf_safe(&wcsfn, "%s/%s%s", wcs_dirs[i], basefn, (fullfilename ? "" : ".wcs"));
			if (file_readable(wcsfn))
				break;
			free(wcsfn);
			wcsfn = NULL;
		}
		if (!wcsfn) {
			logmsg("Failed to find WCS file with basename \"%s\".\n", basefn);
			goto nextfile;
		}

        hdr = qfits_header_read(wcsfn);
        if (!hdr) {
            logmsg("failed to read WCS header from %s\n", wcsfn);
			goto nextfile;
        }
        free(wcsfn);
		wcsfn = NULL;
        res = sip_read_header(hdr, &wcs);
        qfits_header_destroy(hdr);
        if (!res) {
            logmsg("failed to parse SIP header from %s\n", wcsfn);
			goto nextfile;
        }
        W = wcs.wcstan.imagew;
        H = wcs.wcstan.imageh;

		{
			// bottom, right, top, left, close.
			int offsetx[] = { 0, W, W, 0, 0 };
			int offsety[] = { 0, 0, H, H, 0 };
			int stepx[] = { 10, 0, -10, 0, 0 };
			int stepy[] = { 0, 10, 0, -10, 0 };
			int Nsteps[] = { W/10, H/10, W/10, H/10, 1 };
			int side;
			double lastx=0, lasty=0;
			double lastra = 180.0;
			bool lastvalid = FALSE;

			for (side=0; side<5; side++) {
				for (i=0; i<Nsteps[side]; i++) {
					int xin, yin;
					double xout, yout;
					double ra, dec;
					bool first = (!side && !i);
					bool wrapped;
					bool thisvalid;

					xin = offsetx[side] + i * stepx[side];
					yin = offsety[side] + i * stepy[side];
					sip_pixelxy2radec(&wcs, xin, yin, &ra, &dec);
					xout = ra2pixelf(ra, args);
					yout = dec2pixelf(dec, args);
					thisvalid = (yout > 0 && xout > 0 && yout < args->H && xout < args->W);
					wrapped = ((lastra < 90 && ra > 270) || 
							   (lastra > 270 && ra < 90));

					if (wrapped) {
						logmsg("Wrapped: lastra=%g, ra=%g, thisvalid=%i, lastvalid=%i, first=%i.\n",
							   lastra, ra, thisvalid, lastvalid, first);
					}

					if (thisvalid && !lastvalid && !first && !wrapped)
						cairo_move_to(cairo, lastx, lasty);
					if (thisvalid)
						if (!wrapped)
							cairo_line_to(cairo, xout, yout);
					if (!thisvalid && lastvalid) {
						if (!wrapped)
							cairo_line_to(cairo, xout, yout);
						cairo_stroke(cairo);
					}

					if (wrapped)
						thisvalid = FALSE;
					/*
					  if (!side && !i) {
					  cairo_move_to(cairo, xout, yout);
					  } else {
					  cairo_line_to(cairo, xout, yout);
					  }
					*/
					/*
					  if ((thisvalid || lastvalid) && (side || i)) {
					  cairo_move_to(cairo, lastx, lasty);
					  cairo_line_to(cairo, xout, yout);
					  cairo_stroke(cairo);
					  }
					*/
					lastra = ra;
					lastx = xout;
					lasty = yout;
					lastvalid = thisvalid;
                }
            }
			cairo_stroke(cairo);
		}


		if (args->dashbox > 0.0) {
			double ra, dec;
			double mx, my;
			double dm = 0.5 * args->dashbox;
			double dashes[] = {5, 5};

			// merc coordinate of field center:
			sip_pixelxy2radec(&wcs, 0.5 * W, 0.5 * H, &ra, &dec);
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

	nextfile:
		free(wcsfn);
	}

	sl_free2(wcsfiles);

    cairoutils_argb32_to_rgba(img, args->W, args->H);

	cairo_surface_destroy(target);
	cairo_destroy(cairo);

	return 0;
}
