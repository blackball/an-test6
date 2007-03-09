#include <stdio.h>
#include <math.h>
#include <stdint.h>

#include <cairo/cairo.h>

#include "tilerender.h"
#include "render_boundary.h"
#include "sip_qfits.h"

char* wcs_dirs[] = {
	"/home/gmaps/ontheweb-data/"
};

int render_boundary(unsigned char* img, render_args_t* args) {
	// read wcs.
	sip_t wcs;
	int imw, imh;
	int i;

	fprintf(stderr, "render_boundary: Starting.\n");

	qfits_header* wcshead = NULL;
	for (i=0; i<sizeof(wcs_dirs)/sizeof(char*); i++) {
		char fn[256];
		snprintf(fn, sizeof(fn), "%s/%s", wcs_dirs[i], args->wcsfn);
		fprintf(stderr, "render_boundary: Trying wcs file: %s\n", fn);
		wcshead = qfits_header_read(fn);
		if (wcshead) {
			fprintf(stderr, "render_boundary: wcs opened ok\n");
			break;
		} else {
			fprintf(stderr, "render_boundary: wcs didn't open\n");
		}
	}
	if (!wcshead) {
		fprintf(stderr, "render_boundary: couldn't open any wcs files\n");
		return -1;
	}

	if (!sip_read_header(wcshead, &wcs)) {
		fprintf(stderr, "render_boundary: failed to read WCS file.\n");
		return -1;
	}

	imw = qfits_header_getint(wcshead, "IMAGEW", -1);
	imh = qfits_header_getint(wcshead, "IMAGEH", -1);
	if ((imw == -1) || (imh == -1)) {
		fprintf(stderr, "render_boundary: failed to find IMAGE{W,H} in WCS file.\n");
		return -1;
	}

	{
		cairo_t* cairo;
		cairo_surface_t* target;
		double lw = args->linewidth;
		int i;
		// the line endpoints.
		double ends[8] = {0.0, 0.0, 0.0, imh, imw, imh, imw, 0.0};
		int s, SEGS=10;

		target = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32,
													 args->W, args->H, args->W*4);
		cairo = cairo_create(target);
		cairo_set_line_width(cairo, lw);
		cairo_set_line_join(cairo, CAIRO_LINE_JOIN_BEVEL);
		cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);
		cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
		//cairo_set_source_rgba(cairo, 1.0, 1.0, 1.0, 1.0);

		// Draw the field boundary as a curved line by segmenting it into
		// SEGS pieces, running the SIP transformation on each point.
		for (i=0; i<4; i++) {
			double* ep1 = ends + i*2;
			double* ep2 = ends + ((i+1)%4)*2;
			//fprintf(stderr, "ep1=(%g,%g), ep2=(%g,%g)\n", ep1[0], ep1[1], ep2[0], ep2[1]);
			for (s=0; s<SEGS; s++) {
				double x,y,frac;
				double ra, dec;
				double mx,my;
				double px, py;
				frac = (double)s / (double)(SEGS);
				x = ep1[0] * (1.0 - frac) + ep2[0] * frac;
				y = ep1[1] * (1.0 - frac) + ep2[1] * frac;
				sip_pixelxy2radec(&wcs, x, y, &ra, &dec);
				mx = ra2merc(deg2rad(ra));
				my = dec2merc(deg2rad(dec));
				px = xmerc2pixelf(mx, args);
				py = ymerc2pixelf(my, args);

				if (i==0 && s==0)
					cairo_move_to(cairo, px, py);
				else
					cairo_line_to(cairo, px, py);
			}
		}

		cairo_close_path(cairo);
		cairo_stroke(cairo);

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
	}

	return 0;
}
