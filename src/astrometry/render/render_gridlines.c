#include <stdio.h>
#include <math.h>
#include <sys/param.h>

#include <cairo.h>

#include "tilerender.h"
#include "render_gridlines.h"
#include "cairoutils.h"

int render_gridlines(cairo_t* c2, render_args_t* args) {
	double rastep, decstep;
	int ind;
	double steps[] = { 1, 20.0, 10.0, 6.0, 4.0, 2.5, 1.0, 30./60.0,
					   15.0/60.0, 10.0/60.0, 5.0/60.0, 2./60.0 };
	double ra, dec;
	cairo_t* cairo;
	cairo_surface_t* target;
	double x0, y0;
	cairo_text_extents_t ext;
	double textmargin = 1.0;
	double ralabelstep, declabelstep;

	ind = MAX(1, args->zoomlevel);
	ind = MIN(ind, sizeof(steps)/sizeof(double)-1);
	rastep = decstep = steps[ind];
	ralabelstep = 2. * rastep;
	declabelstep = 2. * decstep;

	fprintf(stderr, "render_gridlines: step %g\n", rastep);

	/*
	 In order to properly do the transparency and text, we render onto a
	 mask image, then squish paint through this mask onto the given image.
	 */
	target = cairo_image_surface_create(CAIRO_FORMAT_A8, args->W, args->H);
	cairo = cairo_create(target);
	cairo_set_line_width(cairo, 1.0);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);
	cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
	cairo_select_font_face(cairo, "DejaVu Sans Mono Book", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	//cairo_set_font_size(cairo, 16);
	cairo_set_font_size(cairo, 14);
	cairo_text_extents(cairo, "0", &ext);
	y0 = ext.height + 2.0 * textmargin;
	x0 = ext.width * 3;

	for (ra = rastep * floor(args->ramin / rastep);
		 ra <= rastep * ceil(args->ramax / rastep);
		 ra += rastep) {
		float x = ra2pixelf(ra, args);
		if (!in_image((int)round(x), 0, args))
			continue;
		// draw the grid line on the nearest pixel... cairo pixel centers are at 0.5
		x = 0.5 + round(x-0.5);
		cairo_move_to(cairo, x, 0);
		cairo_line_to(cairo, x, args->H - y0);
	}
	for (dec = decstep * floor(args->decmin / decstep);
		 dec <= decstep * ceil(args->decmax / decstep);
		 dec += decstep) {
		float y = dec2pixelf(dec, args);
		if (!in_image(0, (int)round(y), args))
			continue;
		y = 0.5 + round(y-0.5);
		cairo_move_to(cairo, x0 + 2.0*textmargin, y);
		cairo_line_to(cairo, args->W, y);
	}
	cairo_stroke(cairo);
	
	cairo_set_source_rgba(c2, 0.8, 0.8, 1.0, 0.6);
	cairo_mask_surface(c2, target, 0, 0);
	cairo_select_font_face(c2, "DejaVu Sans Mono Book", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(c2, 16);
	cairo_set_source_rgba(c2, 0.8, 0.8, 1.0, 1.0);
	for (ra = ralabelstep * floor(args->ramin / ralabelstep);
		 ra <= ralabelstep * ceil(args->ramax / ralabelstep);
		 ra += ralabelstep) {
		char buf[32];
		float x = ra2pixelf(ra, args);
		if (!in_image((int)round(x+0.5), 0, args))
			continue;
		sprintf(buf, "%i", (int)ra);
		cairo_text_extents(c2, buf, &ext);
		cairo_move_to(c2, x - (ext.width - ext.x_bearing)/2, args->H - (ext.height + ext.y_bearing) - textmargin);
		cairo_show_text(c2, buf);
	}
	for (dec = declabelstep * floor(args->decmin / declabelstep);
		 dec <= declabelstep * ceil(args->decmax / declabelstep);
		 dec += declabelstep) {
		char buf[32];
		float y = dec2pixelf(dec, args);
		// yep, it can wrap around :)
		if ((dec > 90) || (dec < -90))
			continue;
		if (!in_image(0, (int)round(y+0.5), args))
			continue;
		sprintf(buf, "%i", (int)dec);
		cairo_text_extents(c2, buf, &ext);
		cairo_move_to(c2, textmargin, y - ext.y_bearing/2.0);
		cairo_show_text(c2, buf);
	}

	cairo_surface_destroy(target);
	cairo_destroy(cairo);

	return 0;
}
