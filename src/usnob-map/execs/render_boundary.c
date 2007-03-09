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
	double coords[8];
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

	sip_pixelxy2radec(&wcs, 0.0, 0.0, coords+0, coords+1);
	sip_pixelxy2radec(&wcs, 0.0, imh, coords+2, coords+3);
	sip_pixelxy2radec(&wcs, imw, imh, coords+4, coords+5);
	sip_pixelxy2radec(&wcs, imw, 0.0, coords+6, coords+7);

	{
		cairo_t* cairo;
		cairo_surface_t* target;
		uint32_t* pixels;
		double lw = 2.0;
		int i,j;

		target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, args->W, args->H);
		cairo = cairo_create(target);
		cairo_set_line_width(cairo, lw);
		cairo_set_line_join(cairo, CAIRO_LINE_JOIN_BEVEL);
		cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);
		cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);

		for (i=0; i<4; i++) {
			int x, y;
			double mx = ra2merc(deg2rad(coords[i*2]));
			double my = dec2merc(deg2rad(coords[i*2+1]));
			x = xmerc2pixel(mx, args);
			y = ymerc2pixel(my, args);
			if (i==0)
				cairo_move_to(cairo, x, y);
			else
				cairo_line_to(cairo, x, y);
		}

		cairo_close_path(cairo);
		cairo_stroke(cairo);

		pixels = (uint32_t*)cairo_image_surface_get_data(target);

		for (i=0; i<args->H; i++) {
			uint32_t* row = (uint32_t*)((unsigned char*)pixels + i * cairo_image_surface_get_stride(target));
			for (j=0; j<args->W; j++) {
				unsigned char a,r,g,b;
				a = (row[j] >> 24) & 0xff;
				r = (row[j] >> 16) & 0xff;
				g = (row[j] >>  8) & 0xff;
				b = (row[j]      ) & 0xff;
				img[4*(i*args->W + j) + 3] = a;
				img[4*(i*args->W + j) + 2] = r;
				img[4*(i*args->W + j) + 1] = g;
				img[4*(i*args->W + j) + 0] = b;
			}
		}

		cairo_surface_destroy(target);
		cairo_destroy(cairo);
	}

	return 0;
}
