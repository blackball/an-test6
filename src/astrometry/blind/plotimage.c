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
#include <string.h>
#include <math.h>
#include <sys/param.h>

#include "plotimage.h"
#include "cairoutils.h"
#include "ioutils.h"
#include "sip_qfits.h"
#include "log.h"
#include "errors.h"
#include "anwcs.h"
#include "permutedsort.h"
#include "wcs-resample.h"

static int plot_image_read(const plot_args_t* pargs, plotimage_t* args);

/*
const plotter_t plotter_image = {
	.name = "image",
	.init = plot_image_init,
	.command = plot_image_command,
	.doplot = plot_image_plot,
	.free = plot_image_free
};
 */
/*
plotter_t* plot_image_new() {
	plotter_t* p = calloc(1, sizeof(plotter_t));
	p->name = "image";
	p->init = plot_image_init;
	p->command = plot_image_command;
	p->doplot = plot_image_plot;
	p->free = plot_image_free;
	return p;
}
 */
DEFINE_PLOTTER(image);

static void set_format(plotimage_t* args) {
	if (args->format == 0) {
		assert(args->fn);
		args->format = guess_image_format_from_filename(args->fn);
		logverb("Guessing format of image from filename: \"%s\" -> %s\n", args->fn, image_format_name_from_code(args->format));
	}
}

plotimage_t* plot_image_get(plot_args_t* pargs) {
	return plotstuff_get_config(pargs, "image");
}

void* plot_image_init(plot_args_t* plotargs) {
	plotimage_t* args = calloc(1, sizeof(plotimage_t));
	args->gridsize = 50;
	args->alpha = 1;
	args->image_null = 1.0 / 0.0;
	//args->scalex = args->scaley = 1.0;
	return args;
}

void plot_image_add_to_pixels(plotimage_t* args, int rgb[3]) {
	int i, j, N;
	assert(args->img);
	N = args->W * args->H;
	for (i=0; i<N; i++)
		for (j=0; j<3; j++)
			args->img[i*4+j] = (unsigned char)MIN(255, MAX(0, ((int)args->img[i*4+j]) + rgb[j]));
}

int plot_image_get_percentile(plot_args_t* pargs, plotimage_t* args,
							   double percentile,
							   unsigned char* rgb) {
	int j;
	int N;
	int I;
	if (percentile < 0.0 || percentile > 1.0) {
		ERROR("percentile must be between 0 and 1 (ok, so it's badly named, sue me)");
		return -1;
	}

	if (!args->img) {
		if (plot_image_read(pargs, args)) {
			return -1;
		}
	}

	N = args->W * args->H;
	I = MAX(0, MIN(N-1, floor(N * percentile)));
	for (j=0; j<3; j++) {
		int* P;
		P = permuted_sort(args->img + j, 4, compare_uchars_asc, NULL, N);
		rgb[j] = args->img[4 * P[I] + j];
		free(P);
	}
	return 0;
}

//void plot_image_rgba_data(cairo_t* cairo, unsigned char* img, int W, int H, double alpha) 

void plot_image_rgba_data(cairo_t* cairo, plotimage_t* args) {
	cairo_surface_t* thissurf;
	cairo_pattern_t* pat;
	cairoutils_rgba_to_argb32(args->img, args->W, args->H);
	thissurf = cairo_image_surface_create_for_data(args->img, CAIRO_FORMAT_ARGB32, args->W, args->H, args->W*4);
	pat = cairo_pattern_create_for_surface(thissurf);
	cairo_save(cairo);
	cairo_set_source(cairo, pat);
	//cairo_scale(cairo, args->scalex, args->scaley);
	cairo_paint_with_alpha(cairo, args->alpha);
	cairo_pattern_destroy(pat);
	cairo_surface_destroy(thissurf);
	cairo_restore(cairo);
}

void plot_image_wcs(cairo_t* cairo, unsigned char* img, int W, int H,
					plot_args_t* pargs, plotimage_t* args) {
	cairo_surface_t* thissurf;
	cairo_pattern_t* pat;
	cairo_matrix_t mat;
	int i,j;
	double *xs, *ys;
	int NX, NY;
	double x,y;

	if (args->resample) {
		assert(args->img);

		// FITS images get resampled right after reading.  Others...
		if (args->format != PLOTSTUFF_FORMAT_FITS) {
			// resample onto the output grid...
			unsigned char* img2 = NULL;
			unsigned char* tmpimg = NULL;
			float* rimg = NULL;
			float* fimg = NULL;
			int Nin = args->W * args->H;
			int Nout = pargs->W * pargs->H;

			img2 = calloc(Nout * 4, 1);
			fimg = malloc(Nin * sizeof(float));
			rimg = calloc(Nout, sizeof(float));
			// One plane at a time...
			for (j=0; j<3; j++) {
				// convert the image to float...
				for (i=0; i<Nin; i++)
					fimg[i] = args->img[4*i + j];
				// resample onto output grid...
				if (resample_wcs(args->wcs, fimg, args->W, args->H,
								 pargs->wcs, rimg, pargs->W, pargs->H)) {
					ERROR("Failed to resample image");
					return;
				}
				// convert resampled image back to uchar.
				for (i=0; i<Nout; i++)
					img2[4*i + j] = rimg[i];
			}
			free(rimg);
			free(fimg);

			args->W = pargs->W;
			args->H = pargs->H;
			tmpimg = args->img;
			args->img = img2;
			plot_image_rgba_data(cairo, args);
			args->img = tmpimg;
			free(img2);
		} else {
			plot_image_rgba_data(cairo, args);
		}
		return;
	}

	cairoutils_rgba_to_argb32(img, W, H);
	thissurf = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32, W, H, W*4);

	cairoutils_surface_status_errors(thissurf);
	cairoutils_cairo_status_errors(cairo);

	// Are we double-applying alpha?
	if (args->alpha != 1.0) {
		unsigned char a = MIN(255, MAX(0, args->alpha * 255));
		for (i=0; i<(W*H); i++)
			img[i*4+3] = a;
		cairoutils_premultiply_alpha_rgba(img, W, H);
	}

	pat = cairo_pattern_create_for_surface(thissurf);
	cairoutils_cairo_status_errors(cairo);

	assert(args->gridsize >= 1);
	NX = 1 + ceil(W / args->gridsize);
	NY = 1 + ceil(H / args->gridsize);
	xs = malloc(NX*NY * sizeof(double));
	ys = malloc(NX*NY * sizeof(double));

	// FIXME -- NEAREST is good when we're zooming in on individual pixels;
	// some smoothing is necessary if we're zoomed out.  Should probably
	// resample image in this case, since I doubt cairo is very smart in this case.
	cairo_pattern_set_filter(pat, CAIRO_FILTER_GOOD);
							 //CAIRO_FILTER_NEAREST);
	for (j=0; j<NY; j++) {
		double ra,dec;
		y = MIN(j * args->gridsize, H-1);
		for (i=0; i<NX; i++) {
			double ox,oy;
			bool ok;
			x = MIN(i * args->gridsize, W-1);
			anwcs_pixelxy2radec(args->wcs, x+1, y+1, &ra, &dec);
			ok = plotstuff_radec2xy(pargs, ra, dec, &ox, &oy);
			xs[j*NX+i] = ox-1;
			ys[j*NX+i] = oy-1;
			debug("image (%.1f,%.1f) -> radec (%.4f,%.4f), plot (%.1f,%.1f)\n", x, y, ra, dec, xs[j*NX+i], ys[j*NX+i]);
		}
	}
	cairo_save(cairo);
	cairo_set_source(cairo, pat);
    //cairo_set_source_rgb(cairo, 1,0,0);
	for (j=0; j<(NY-1); j++) {
		for (i=0; i<(NX-1); i++) {
			int aa = j*NX + i;
			int ab = aa + 1;
			int ba = aa + NX;
			int bb = aa + NX + 1;
            double midx = (xs[aa] + xs[ab] + xs[bb] + xs[ba])*0.25;
            double midy = (ys[aa] + ys[ab] + ys[bb] + ys[ba])*0.25;
			cairo_status_t st;

            double xlo,xhi,ylo,yhi;
            ylo = MIN(j     * args->gridsize, H-1);
            yhi = MIN((j+1) * args->gridsize, H-1);
			xlo = MIN(i     * args->gridsize, W-1);
			xhi = MIN((i+1) * args->gridsize, W-1);
			if (xlo == xhi || ylo == yhi)
				continue;

            cairo_move_to(cairo,
                          0.5 + xs[aa]+0.5*(xs[aa] >= midx ? 1 : -1),
                          0.5 + ys[aa]+0.5*(ys[aa] >= midy ? 1 : -1));
            cairo_line_to(cairo,
                          0.5 + xs[ab]+0.5*(xs[ab] >= midx ? 1 : -1),
                          0.5 + ys[ab]+0.5*(ys[ab] >= midy ? 1 : -1));
            cairo_line_to(cairo,
                          0.5 + xs[bb]+0.5*(xs[bb] >= midx ? 1 : -1),
                          0.5 + ys[bb]+0.5*(ys[bb] >= midy ? 1 : -1));
            cairo_line_to(cairo,
                          0.5 + xs[ba]+0.5*(xs[ba] >= midx ? 1 : -1),
                          0.5 + ys[ba]+0.5*(ys[ba] >= midy ? 1 : -1));
			cairo_close_path(cairo);
			cairo_matrix_init(&mat,
							  (xs[ab]-xs[aa])/(xhi-xlo),
							  (ys[ab]-ys[aa])/(yhi-ylo),
							  (xs[ba]-xs[aa])/(xhi-xlo),
							  (ys[ba]-ys[aa])/(yhi-ylo),
							  xs[0], ys[0]);
			st = cairo_matrix_invert(&mat);
			if (st != CAIRO_STATUS_SUCCESS) {
				ERROR("Cairo: %s", cairo_status_to_string(st));
				ERROR("Matrix: AB %g, %g, BA %g, %g, AA %g, %g\n"
					  "  xlo,xhi %g, %g  ylo,yhi %g, %g",
					  xs[ab], ys[ab], xs[ba], ys[ba], xs[aa], ys[aa],
					  xlo, xhi, ylo, yhi);
				// Matrix: AB 270.892, 121.737, BA 274.958, 129.407, AA 274.958, 129.407
				//  xlo,xhi 0, 50  ylo,yhi 2050, 2050					  
				continue;
			}

			cairo_pattern_set_matrix(pat, &mat);
			cairo_fill(cairo);
		}
	}
	/* Grid:
	 cairo_set_source_rgb(cairo, 1,0,0);
	 for (j=0; j<(NY-1); j++) {
	 for (i=0; i<(NX-1); i++) {
	 int aa = j*NX + i;
	 int ab = aa + 1;
	 int ba = aa + NX;
	 int bb = aa + NX + 1;
	 cairo_move_to(cairo, xs[aa], ys[aa]);
	 cairo_line_to(cairo, xs[ab], ys[ab]);
	 cairo_line_to(cairo, xs[bb], ys[bb]);
	 cairo_line_to(cairo, xs[ba], ys[ba]);
	 cairo_close_path(cairo);
	 cairo_stroke(cairo);
	 }
	 }
	 {
	 int aa = 0;
	 int ab = 1;
	 int ba = NX;
	 int bb = NX + 1;
	 cairo_set_source_rgb(cairo, 0,1,0);
	 cairo_move_to(cairo, xs[aa], ys[aa]);
	 cairo_line_to(cairo, xs[ab], ys[ab]);
	 cairo_line_to(cairo, xs[bb], ys[bb]);
	 cairo_line_to(cairo, xs[ba], ys[ba]);
	 cairo_close_path(cairo);
	 cairo_move_to(cairo, 0, 0);
	 cairo_line_to(cairo, 0, args->gridsize);
	 cairo_line_to(cairo, args->gridsize, args->gridsize);
	 cairo_line_to(cairo, args->gridsize, 0);
	 cairo_close_path(cairo);
	 cairo_stroke(cairo);
	 }
	 */

	free(xs);
	free(ys);

	cairo_pattern_destroy(pat);
	cairo_surface_destroy(thissurf);
	cairo_restore(cairo);
}

static unsigned char* read_fits_image(const plot_args_t* pargs, plotimage_t* args) {
	float* fimg;
	qfitsloader ld;
	unsigned char* img;
	int i,j;
	float offset, scale;
	float* rimg = NULL;

	ld.filename = args->fn;
	ld.xtnum = args->fitsext;
	ld.pnum = args->fitsplane;
	ld.map = 1;
	ld.ptype = PTYPE_FLOAT;
		
	if (qfitsloader_init(&ld)) {
		ERROR("qfitsloader_init() failed");
		return NULL;
	}
	if (qfits_loadpix(&ld)) {
		ERROR("qfits_loadpix() failed");
		return NULL;
	}
	args->W = ld.lx;
	args->H = ld.ly;
	fimg = ld.fbuf;

	if (args->resample) {
		// resample onto the output grid...
		rimg = calloc(pargs->W * pargs->H, sizeof(float));
		if (resample_wcs(args->wcs, fimg, args->W, args->H,
						 pargs->wcs, rimg, pargs->W, pargs->H)) {
			ERROR("Failed to resample image");
			return NULL;
		}

		// ?
		args->W = pargs->W;
		args->H = pargs->H;
		fimg = rimg;
	}

	if (args->image_low == 0 && args->image_high == 0) {
		if (args->auto_scale) {
			// min/max, or percentiles?
			/*
			 double mn = HUGE_VAL;
			 double mx = -HUGE_VAL;
			 for (i=0; i<(args->W*args->H); i++) {
			 mn = MIN(mn, fimg[i]);
			 mx = MAX(mx, fimg[i]);
			 }
			 */
			int N = args->W * args->H;
			int* perm = permutation_init(NULL, N);
			int i;
			int Nreal = 0;
			for (i=0; i<N; i++) {
				if (isfinite(fimg[i])) {
					perm[Nreal] = perm[i];
					Nreal++;
				}
			}
			permuted_sort(fimg, sizeof(float), compare_floats_asc, perm, Nreal);
			double mn = fimg[perm[(int)(Nreal * 0.1)]];
			double mx = fimg[perm[(int)(Nreal * 0.98)]];
			logmsg("Image auto-scaling: range %g, %g; percentiles %g, %g\n", fimg[perm[0]], fimg[perm[N-1]], mn, mx);
			free(perm);

			offset = mn;
			scale = (255.0 / (mx - mn));
			logmsg("Image range %g, %g --> offset %g, scale %g\n", mn, mx, offset, scale);
		} else {
			offset = 0.0;
			scale = 1.0;
		}
	} else {
		offset = args->image_low;
		scale = 255.0 / (args->image_high - args->image_low);
		logmsg("Image range %g, %g --> offset %g, scale %g\n", args->image_low, args->image_high, offset, scale);
	}

	img = malloc(args->W * args->H * 4);
	for (j=0; j<args->H; j++) {
		for (i=0; i<args->W; i++) {
			int k;
			unsigned char v;
			double pval = fimg[j*args->W + i];
			k = 4*(j*args->W + i);
			if ((isnan(args->image_null) && isnan(pval)) ||
				(args->image_null == pval)) {
				img[k+0] = 0;
				img[k+1] = 0;
				img[k+2] = 0;
				img[k+3] = 0;
			} else {
				v = MIN(255, MAX(0, (pval - offset) * scale));
				img[k+0] = v;
				img[k+1] = v;
				img[k+2] = v;
				img[k+3] = 255;
			}
		}
	}
	qfitsloader_free_buffers(&ld);

	free(rimg);

	return img;
}

static int plot_image_read(const plot_args_t* pargs, plotimage_t* args) {
	set_format(args);
	switch (args->format) {
	case PLOTSTUFF_FORMAT_JPG:
		args->img = cairoutils_read_jpeg(args->fn, &(args->W), &(args->H));
		break;
	case PLOTSTUFF_FORMAT_PNG:
		args->img = cairoutils_read_png(args->fn, &(args->W), &(args->H));
		break;
	case PLOTSTUFF_FORMAT_PPM:
		args->img = cairoutils_read_ppm(args->fn, &(args->W), &(args->H));
		break;
	case PLOTSTUFF_FORMAT_FITS:
		assert(pargs);
		args->img = read_fits_image(pargs, args);
		break;
	case PLOTSTUFF_FORMAT_PDF:
		ERROR("PDF format not supported");
		return -1;
	default:
		ERROR("You must set the image format with \"image_format <png|jpg|ppm>\"");
		return -1;
	}
	return 0;
}

int plot_image_set_filename(plotimage_t* args, const char* fn) {
	free(args->fn);
	args->fn = strdup_safe(fn);
	return 0;
}

int plot_image_plot(const char* command,
					cairo_t* cairo, plot_args_t* pargs, void* baton) {
	plotimage_t* args = (plotimage_t*)baton;
	// Plot it!
	if (!args->img) {
		if (plot_image_read(pargs, args)) {
			return -1;
		}
	}

	plotstuff_builtin_apply(cairo, pargs);

	if (pargs->wcs && args->wcs) {
		plot_image_wcs(cairo, args->img, args->W, args->H, pargs, args);
	} else {
		plot_image_rgba_data(cairo, args);
	}
	// ?
	free(args->img);
	args->img = NULL;
	return 0;
}

static int read_fits_size(plotimage_t* args, int* W, int* H) {
	qfitsloader ld;
	ld.filename = args->fn;
	ld.xtnum = args->fitsext;
	ld.pnum = args->fitsplane;
	ld.map = 1;
	ld.ptype = PTYPE_FLOAT;
	if (qfitsloader_init(&ld)) {
		ERROR("qfitsloader_init() failed");
		return -1;
	}
	if (W)
		*W = ld.lx;
	if (H)
		*H = ld.ly;
	qfitsloader_free_buffers(&ld);
	return 0;
}

int plot_image_getsize(plotimage_t* args, int* W, int* H) {
	set_format(args);
	if (args->format == PLOTSTUFF_FORMAT_FITS)
		return read_fits_size(args, W, H);
	if (!args->img) {
		// HACK -- only FITS format needs pargs.
		if (plot_image_read(NULL, args)) {
			return -1;
		}
	}
	if (W)
		*W = args->W;
	if (H)
		*H = args->H;
	return 0;
}

int plot_image_setsize(plot_args_t* pargs, plotimage_t* args) {
	if (!args->img) {
		if (plot_image_read(pargs, args)) {
			return -1;
		}
	}
	plotstuff_set_size(pargs, args->W, args->H);
	//plotstuff_set_size(pargs, args->W * args->scalex, args->H * args->scaley);
	return 0;
}

int plot_image_set_wcs(plotimage_t* args, const char* filename, int ext) {
	if (args->wcs)
		anwcs_free(args->wcs);
	if (streq(filename, "none")) {
		args->wcs = NULL;
	} else {
		args->wcs = anwcs_open(filename, ext);
		if (!args->wcs) {
			ERROR("Failed to read WCS file \"%s\"", filename);
			return -1;
		}
		if (log_get_level() >= LOG_VERB) {
			logverb("Set image WCS to:");
			anwcs_print(args->wcs, stdout);
		}
	}
	return 0;
}

int plot_image_command(const char* cmd, const char* cmdargs,
					   plot_args_t* pargs, void* baton) {
	plotimage_t* args = (plotimage_t*)baton;
	if (streq(cmd, "image_file")) {
		plot_image_set_filename(args, cmdargs);
	} else if (streq(cmd, "image_alpha")) {
		args->alpha = atof(cmdargs);
	} else if (streq(cmd, "image_format")) {
		args->format = parse_image_format(cmdargs);
		if (args->format == -1)
			return -1;
	} else if (streq(cmd, "image_setsize")) {
		if (plot_image_setsize(pargs, args))
			return -1;
	} else if (streq(cmd, "image_wcslib")) {
		// force reading WCS using WCSLIB.
		if (args->wcs)
			anwcs_free(args->wcs);
		args->wcs = anwcs_open_wcslib(cmdargs, 0);
		if (!args->wcs) {
			ERROR("Failed to read WCS file \"%s\"", cmdargs);
			return -1;
		}
		if (log_get_level() >= LOG_VERB) {
			logverb("Set image WCS to:");
			anwcs_print(args->wcs, stdout);
		}
	} else if (streq(cmd, "image_wcs")) {
		return plot_image_set_wcs(args, cmdargs, args->fitsext);
	} else if (streq(cmd, "image_ext")) {
		args->fitsext = atoi(cmdargs);
	} else if (streq(cmd, "image_grid")) {
		args->gridsize = atof(cmdargs);
	} else if (streq(cmd, "image_low")) {
		args->image_low = atof(cmdargs);
		logmsg("set image_low %g\n", args->image_low);
	} else if (streq(cmd, "image_null")) {
		args->image_null = atof(cmdargs);
	} else if (streq(cmd, "image_high")) {
		args->image_high = atof(cmdargs);
		logmsg("set image_high %g\n", args->image_high);
	} else {
		ERROR("Did not understand command \"%s\"", cmd);
		return -1;
	}
	return 0;
}

void plot_image_free(plot_args_t* plotargs, void* baton) {
	plotimage_t* args = (plotimage_t*)baton;
	if (args->wcs)
		anwcs_free(args->wcs);
	free(args->fn);
	free(args);
}

