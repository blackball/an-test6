#include <stdio.h>
#include <math.h>
#include <stdarg.h>

#include "tilerender.h"
#include "render_rdls.h"
#include "rdlist.h"
#include "starutil.h"
#include "mathutil.h"
#include "mercrender.h"

char* rdls_dirs[] = {
    "/home/gmaps/gmaps-rdls/",
    "/home/gmaps/ontheweb-data/",
};

static void logmsg(char* format, ...) {
	va_list args;
	va_start(args, format);
	fprintf(stderr, "render_rdls: ");
	vfprintf(stderr, format, args);
	va_end(args);
}

int render_rdls(unsigned char* img, render_args_t* args)
{
    int i, j, Nstars, Nib=0;
    xylist* rdls;
    double* rdvals = NULL;
	int r;
	cairo_t* cairo;
	cairo_surface_t* target;

	// draw black outline?
	bool outline = TRUE;

	target = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32,
												 args->W, args->H, args->W*4);
	cairo = cairo_create(target);
	cairo_set_line_join(cairo, CAIRO_LINE_JOIN_BEVEL);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);

	for (r=0; r<pl_size(args->rdlsfns); r++) {
		char* rdlsfn = pl_get(args->rdlsfns, r);
		char* color = pl_get(args->rdlscolors, r);
		int fieldnum = il_get(args->fieldnums, r);
		int maxstars = il_get(args->Nstars, r);
		//double lw = dl_get(args->rdlslws, r);
		double lw = 2.0;
		double rad = 3.0;
		char style = 'o';
		double r, g, b;

		cairo_set_line_width(cairo, lw);
		r = g = b = 1.0;

		for (j=0; color && j<strlen(color); j++) {
			switch (color[j]) {
			case 'r': // red
				r = 1.0;
				g = b = 0.0;
				break;
			case 'b': // blue
				r = g = 0.0;
				b = 1.0;
				break;
			case 'm': // magenta
				r = b = 1.0;
				g = 0.0;
				break;
			case 'y': // yellow
				r = g = 1.0;
				b = 0.0;
				break;
			case 'g': // green
				r = b = 0.0;
				g = 1.0;
				break;
			case 'c': // cyan
				r = 0.0;
				g = b = 1.0;
				break;
			case 'w': // white
				r = g = b = 1.0;
				break;

			case 'p':
			case '+': // plus-sign
				style = '+';
				break;
			case 'h':
			case '#': // box
				style = '#';
				break;
			case 'o': // circle
				style = 'o';
				break;
			}
		}
		cairo_set_source_rgb(cairo, r, g, b);

		
		/* Search in the rdls paths */
		for (i=0; i<sizeof(rdls_dirs)/sizeof(char*); i++) {
			char fn[256];
			snprintf(fn, sizeof(fn), "%s/%s", rdls_dirs[i], rdlsfn);
			logmsg("Trying file: %s\n", fn);
			rdls = rdlist_open(fn);
			if (rdls)
				break;
		}
		if (!rdls) {
			logmsg("Failed to open RDLS file \"%s\".\n", rdlsfn);
			return -1;
		}

		if (fieldnum < 1)
			fieldnum = 1;
		Nstars = rdlist_n_entries(rdls, fieldnum);
		if (Nstars == -1) {
			logmsg("Failed to read RDLS file. \"%s\"\n", rdlsfn);
			return -1;
		}

		if (maxstars && Nstars > maxstars)
			Nstars = maxstars;

		rdvals = malloc(Nstars * 2 * sizeof(double));
		if (rdlist_read_entries(rdls, fieldnum, 0, Nstars, rdvals)) {
			logmsg("Failed to read RDLS file. \"%s\"\n", rdlsfn);
			free(rdvals);
			return -1;
		}
		logmsg("Got %i stars.\n", Nstars);

		Nib = 0;
		for (i=0; i<Nstars; i++) {
			double px, py;

			px =  ra2pixelf(rdvals[i*2+0], args);
			py = dec2pixelf(rdvals[i*2+1], args);

			if (!in_image_margin(px, py, rad, args))
				continue;

			// cairo has funny ideas about pixel coordinates...
			px += 0.5;
			py += 0.5;

			switch (style) {
			case 'o':
				if (outline) {
					cairo_set_line_width(cairo, lw + 2.0);
					cairo_set_source_rgb(cairo, 0, 0, 0);
					cairo_arc(cairo, px, py, rad, 0.0, 2.0*M_PI);
					cairo_stroke(cairo);
					cairo_set_line_width(cairo, lw);
					cairo_set_source_rgb(cairo, r, g, b);
				}
				cairo_arc(cairo, px, py, rad, 0.0, 2.0*M_PI);

				break;

			case '+':
				cairo_move_to(cairo, px-rad, py);
				cairo_line_to(cairo, px+rad, py);
				cairo_move_to(cairo, px, py-rad);
				cairo_line_to(cairo, px, py+rad);
				break;

			case '#':
				cairo_move_to(cairo, px-rad, py-rad);
				cairo_line_to(cairo, px-rad, py+rad);
				cairo_line_to(cairo, px+rad, py+rad);
				cairo_line_to(cairo, px+rad, py-rad);
				cairo_line_to(cairo, px-rad, py-rad);
				break;
			}

			cairo_stroke(cairo);

			Nib++;
		}
    }
    logmsg("%i stars inside image bounds.\n", Nib);

	// Cairo's uint32 ARGB32 format is a little different than what we need,
	// which is uchar R,G,B,A.
	for (i=0; i<(args->H*args->W); i++) {
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

	cairo_surface_destroy(target);
	cairo_destroy(cairo);

    free(rdvals);

    return 0;
}
