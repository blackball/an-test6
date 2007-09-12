#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <sys/mman.h>

#include "tilerender.h"
#include "starutil.h"
#include "mathutil.h"
#include "merctree.h"
#include "keywords.h"
#include "mercrender.h"
#include "render_clean.h"

#define max(a, b)  ((a)>(b)?(a):(b))
#define min(a, b)  ((a)<(b)?(a):(b))

static char* merc_template  = "/data1/usnob-gmaps/merc-clean-20070909/merc_%02i_%02i.mkdt.fits";
static char* prerendered_template = "/data1/usnob-gmaps/prerendered-clean-20070909/zoom%i/usnob_z%1$i_%02i_%02i.raw";
static char* merc_template_newest  = "/data1/usnob-gmaps/merc-clean-20070911/merc_%02i_%02i.mkdt.fits";
static char* prerendered_template_newest = "/data1/usnob-gmaps/prerendered-clean-20070911/zoom%i/usnob_z%1$i_%02i_%02i.raw";

// Gridding of Mercator space
static int NM = 32;
// Max pre-rendered zoom level
//static int PRERENDERED = 5;
static int PRERENDERED = -1;

static void logmsg(char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "render_clean: ");
    vfprintf(stderr, format, args);
    va_end(args);
}

static void map_flux(unsigned char* img, render_args_t* args,
					 double rflux, double bflux, double nflux,
					 int x, int y, double amp) {
	unsigned char* pix;
	double r, g, b, I, f, R, G, B, maxRGB;
    double meanRGB;
    double flux;

    flux = (rflux + bflux + nflux);
    if (flux > 0)
        flux /=
            ((rflux > 0.0 ? 1.0 : 0.0) + 
             (bflux > 0.0 ? 1.0 : 0.0) + 
             (nflux > 0.0 ? 1.0 : 0.0));
    r = b = g = flux;

	I = (r + g + b) / 3;
	if (I == 0.0) {
		R = G = B = 0.0;
        meanRGB = 0.0;
	} else {
		if (args->arc) {
			f = asinh(I * amp);
		} else {
			f = pow(I * amp, 0.25);
		}
		R = f*r/I;
		G = f*g/I;
		B = f*b/I;

        meanRGB = (R + G + B) / 3.0;
        if (meanRGB > 1.0) meanRGB = 1.0;

		maxRGB = max(R, max(G, B));
		//if (maxRGB > 1.0) {
        R /= maxRGB;
        G /= maxRGB;
        B /= maxRGB;
        //}
	}
	pix = pixel(x, y, img, args);

    pix[0] = min(255, 255.0*R);
    pix[1] = min(255, 255.0*G);
    pix[2] = min(255, 255.0*B);
    pix[3] = min(255, 255.0*meanRGB);
    /*
     pix[0] = min(255, 255.0*R);
     pix[1] = min(255, 255.0*G);
     pix[2] = min(255, 255.0*B);
     pix[3] = 255; //(pix[0]/3 + pix[1]/3 + pix[2]/3);
     */
}

int render_clean(unsigned char* img, render_args_t* args) {
    float* fluximg;
    float amp = 0.0;
    int i, j;
    int xlo, xhi, ylo, yhi;
    int tmp;

    logmsg("hello world\n");

    xlo = (int)(NM * args->xmercmin);
    xhi = (int)(NM * args->xmercmax);
    ylo = (int)(NM * args->ymercmin);
    yhi = (int)(NM * args->ymercmax);
    logmsg("reading tiles x:[%i, %i], y:[%i, %i]\n", xlo, xhi, ylo, yhi);
    // clamp.
    xlo = min(NM-1, max(0, xlo));
    xhi = min(NM-1, max(0, xhi));
    ylo = min(NM-1, max(0, ylo));
    yhi = min(NM-1, max(0, yhi));
    if (xlo > xhi) {
        logmsg("xlo > xhi: %i, %i\n", xlo, xhi);
        tmp = xlo;
        xlo = xhi;
        xhi = tmp;
    }
    if (ylo > yhi) {
        logmsg("ylo > yhi: %i, %i\n", ylo, yhi);
        tmp = ylo;
        ylo = yhi;
        yhi = tmp;
    }

    logmsg("reading tiles x:[%i, %i], y:[%i, %i]\n", xlo, xhi, ylo, yhi);

    amp = pow(4.0, min(5, args->zoomlevel)) * 32.0 * exp(args->gain * log(4.0));

	if (!args->nopre && args->zoomlevel <= PRERENDERED) {
		char fn[1024];
		int n = (1 << args->zoomlevel);
		// assume NM = 2^(PRERENDERED+1) ?
		int block = NM/n;
		// HACK...
		int WH = 256;
		FILE* f;
		size_t mapsize = WH * WH * 3 * sizeof(float);
		void* map;
		float* flux;
		double mxstep, mystep;
		int xtlo, xthi, ytlo, ythi;

		logmsg("using prerendered zoom level %i. (n=%i)\n", args->zoomlevel, n);

		xtlo = xlo/block;
		xthi = xhi/block;
		ytlo = ylo/block;
		ythi = yhi/block;

		logmsg("using prerendered tiles x:[%i,%i], y:[%i,%i]\n", xtlo, xthi, ytlo, ythi);

		mxstep = 1 / (double)(n * WH);
		mystep = -mxstep;

		for (i=xtlo; i<=xthi; i++) {
			for (j=ytlo; j<=ythi; j++) {
				double mx, my;
				int xp, yp;
				// merc position of the first pixel of this pre-rendered tile.
				mx = i / (double)n;
				my = (j+1) / (double)n;
				my += mystep;
				logmsg("  (%i,%i): merc (%g,%g)\n", i, j, mx, my);
				logmsg("  merc x step %g (%g for %i pixels)\n", mxstep, mxstep*WH, WH);
				logmsg("  merc y step %g (%g for %i pixels)\n", mystep, mystep*WH, WH);

                snprintf(fn, sizeof(fn),
						 (args->newest ? prerendered_template_newest : prerendered_template),
						 args->zoomlevel, i, j);
				logmsg("  reading file %s\n", fn);
				f = fopen(fn, "rb");
				if (!f) {
					logmsg("failed to read prerendered file %s\n", fn);
					return -1;
				}
				map = mmap(NULL, mapsize, PROT_READ, MAP_SHARED, fileno(f), 0);
				fclose(f);
				if (map == MAP_FAILED) {
					logmsg("failed to mmap file %s\n", fn);
					return -1;
				}
				flux = map;

				logmsg("  pixel (0,0) of this tile goes to (%i,%i) in the image.\n",
					   xmerc2pixel(mx, args), ymerc2pixel(my, args));
				logmsg("  pixel (0,0) of this tile goes to (%g,%g) in the image.\n",
					   xmerc2pixelf(mx, args), ymerc2pixelf(my, args));
				logmsg("  pixel (%i,%i) of this tile goes to (%i,%i) in the image.\n",
					   WH-1, WH-1, xmerc2pixel(mx+mxstep*(WH-1), args), ymerc2pixel(my+mystep*(WH-1), args));
				logmsg("  pixel (%i,%i) of this tile goes to (%g,%g) in the image.\n",
					   WH-1, WH-1, xmerc2pixelf(mx+mxstep*(WH-1), args), ymerc2pixelf(my+mystep*(WH-1), args));

				for (yp=0; yp<WH; yp++) {
					int ix, iy;
					iy = (int)round(ymerc2pixelf(my + mystep * yp, args));
					for (xp=0; xp<WH; xp++) {
						double r,b,n;

						ix = (int)round(xmerc2pixelf(mx + mxstep * xp, args));

						if (!in_image(ix, iy, args))
							continue;

						n = flux[3*(yp*WH + xp) + 0];
						r = flux[3*(yp*WH + xp) + 1];
						b = flux[3*(yp*WH + xp) + 2];

						map_flux(img, args, r, b, n, ix, iy, amp);
					}
				}

				munmap(map, mapsize);
			}
		}
		return 0;
	}

    fluximg = calloc(args->W * args->H * 3, sizeof(float));
    if (!fluximg) {
        logmsg("Failed to allocate flux image.\n");
        return -1;
    }

    for (i=xlo; i<=xhi; i++) {
        for (j=ylo; j<=yhi; j++) {
            char fn[1024];
            merctree* merc;

            logmsg("rendering tile %i, %i.\n", i, j);

            snprintf(fn, sizeof(fn),
					 (args->newest ? merc_template_newest : merc_template),
					 j, i);
            logmsg("reading file %s\n", fn);
            merc = merctree_open(fn);
            if (!merc) {
                logmsg("Failed to open merctree %s\n", fn);
				continue;
                //return -1;
            }
            mercrender(merc, args, fluximg, RENDERSYMBOL_psf);
            merctree_close(merc);
        }
    }

    if (args->makerawfloatimg) {
        args->rawfloatimg = fluximg;
        // shuffle channels; see below about RBN order.
        for (j=0; j<(args->H*args->W); j++) {
            float r,g,b;
            r = fluximg[3*j + 2];
            g = fluximg[3*j + 0];
            b = fluximg[3*j + 1];
            fluximg[3*j + 0] = r;
            fluximg[3*j + 1] = g;
            fluximg[3*j + 2] = b;
        }
        return 0;
    }

    for (j=0; j<args->H; j++) {
        for (i=0; i<args->W; i++) {
			double r,b,n;

			r = fluximg[3*(j*args->W + i) + 0];
			b = fluximg[3*(j*args->W + i) + 1];
			n = fluximg[3*(j*args->W + i) + 2];

			map_flux(img, args, r, b, n, i, j, amp);
        }
    }
    free(fluximg);

    return 0;
}

