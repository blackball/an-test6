/*
 This file is part of the Astrometry.net suite.
 Copyright 2009, 2010 Dustin Lang.

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

#ifndef PLOTSTUFF_H
#define PLOTSTUFF_H

#include <stdio.h>
#include <cairo.h>

#include "keywords.h"
#include "sip.h"
#include "bl.h"
#include "anwcs.h"

#define PLOTSTUFF_FORMAT_JPG 1
#define PLOTSTUFF_FORMAT_PNG 2
#define PLOTSTUFF_FORMAT_PPM 3
#define PLOTSTUFF_FORMAT_PDF 4
// Save the image as RGBA image "pargs->outimage"
#define PLOTSTUFF_FORMAT_MEMIMG 5
#define PLOTSTUFF_FORMAT_FITS 6

struct plot_args {
    char* outfn;
	FILE* fout;
	int outformat;

	unsigned char* outimage;

	cairo_t* cairo;
	cairo_surface_t* target;

	cairo_operator_t op;

	anwcs_t* wcs;

	int W, H;
	float rgba[4];
	float lw; // default: 1
	int marker; // default: circle
	float markersize; // default: 5

	float bg_rgba[4];
	float bg_lw; // default: 3

	float fontsize; // default: 20

    double label_offset_x;
    double label_offset_y;

	int text_bg_layer;
	int text_fg_layer;
	int marker_fg_layer;

	bl* cairocmds;

	// step size in pixels for drawing curved lines in RA,Dec
	float linestep;
};
typedef struct plot_args plot_args_t;

typedef void* (*plot_func_init_t)(plot_args_t* args);
typedef int   (*plot_func_init2_t)(plot_args_t* args, void* baton);
typedef int   (*plot_func_command_t)(const char* command, const char* cmdargs, plot_args_t* args, void* baton);
typedef int   (*plot_func_plot_t)(const char* command, cairo_t* cr, plot_args_t* args, void* baton);
typedef void  (*plot_func_free_t)(plot_args_t* args, void* baton);

struct plotter {
	// don't change the order of these fields!
	char* name;
	plot_func_init_t init;
	plot_func_init2_t init2;
	plot_func_command_t command;
	plot_func_plot_t doplot;
	plot_func_free_t free;
	void* baton;
};
typedef struct plotter plotter_t;

// return PLOTSTUFF_FORMAT_*, or -1 on error
int parse_image_format(const char* fmt);
int guess_image_format_from_filename(const char* fn);
const char* image_format_name_from_code(int code);

int parse_color(const char* color, float* r, float* g, float* b, float* a);
int parse_color_rgba(const char* color, float* rgba);
int cairo_set_color(cairo_t* cairo, const char* color);
void cairo_set_rgba(cairo_t* cairo, const float* rgba);


plot_args_t* plotstuff_new(void);
int plotstuff_init(plot_args_t* plotargs);
int plotstuff_read_and_run_command(plot_args_t* pargs, FILE* f);
int plotstuff_run_command(plot_args_t* pargs, const char* cmd);

int plotstuff_plot_layer(plot_args_t* pargs, const char* layer);

void* plotstuff_get_config(plot_args_t* pargs, const char* name);

int plotstuff_set_color(plot_args_t* pargs, const char* name);
int plotstuff_set_bgcolor(plot_args_t* pargs, const char* name);

int plotstuff_set_alpha(plot_args_t* pargs, float alpha);

int plotstuff_set_rgba(plot_args_t* pargs, const float* rgba);

int plotstuff_set_rgba2(plot_args_t* pargs, float r, float g, float b, float a);
int plotstuff_set_bgrgba2(plot_args_t* pargs, float r, float g, float b, float a);

int plotstuff_set_marker(plot_args_t* pargs, const char* name);

int plotstuff_set_markersize(plot_args_t* pargs, double ms);

int plotstuff_set_size(plot_args_t* pargs, int W, int H);

int plotstuff_set_wcs_box(plot_args_t* pargs, float ra, float dec, float width);

int plotstuff_set_wcs(plot_args_t* pargs, anwcs_t* wcs);

int plotstuff_set_wcs_tan(plot_args_t* pargs, tan_t* wcs);

int plotstuff_set_wcs_sip(plot_args_t* pargs, sip_t* wcs);

void plotstuff_builtin_apply(cairo_t* cairo, plot_args_t* args);

// Would a marker plotted with the current markersize at x,y appear in the image?
bool plotstuff_marker_in_bounds(plot_args_t* pargs, double x, double y);

int
ATTRIB_FORMAT(printf,2,3)
plotstuff_run_commandf(plot_args_t* pargs, const char* fmt, ...);

int plotstuff_output(plot_args_t* pargs);
void plotstuff_free(plot_args_t* pargs);


void plotstuff_stack_marker(plot_args_t* pargs, double x, double y);
void plotstuff_stack_arrow(plot_args_t* pargs, double x, double y,
						   double x2, double y2);
void plotstuff_stack_text(plot_args_t* pargs, cairo_t* cairo,
						  const char* txt, double px, double py);
int plotstuff_plot_stack(plot_args_t* pargs, cairo_t* cairo);

/// WCS-related stuff:

// in arcsec/pixel
double plotstuff_pixel_scale(plot_args_t* pargs);

// RA,Dec in degrees
// x,y in pixels (cairo coordinates)
// Returns TRUE on success.
bool plotstuff_radec2xy(plot_args_t* pargs, double ra, double dec,
						double* x, double* y);

int plotstuff_get_radec_center_and_radius(plot_args_t* pargs, double* ra, double* dec, double* radius);

void plotstuff_get_radec_bounds(const plot_args_t* pargs, int stepsize,
								double* pramin, double* pramax,
								double* pdecmin, double* pdecmax);

bool plotstuff_radec_is_inside_image(plot_args_t* pargs, double ra, double dec);

int plot_line_constant_ra(plot_args_t* pargs, double ra, double dec1, double dec2);
int plot_line_constant_dec(plot_args_t* pargs, double dec, double ra1, double ra2);

int plot_text_radec(plot_args_t* pargs, double ra, double dec, const char* label);

int plotstuff_append_doubles(const char* str, dl* lst);

#endif
