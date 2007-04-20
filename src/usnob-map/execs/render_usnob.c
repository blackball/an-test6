#include <stdio.h>
#include <math.h>
#include <stdarg.h>

#include "tilerender.h"
#include "starutil.h"
#include "mathutil.h"
#include "merctree.h"
#include "keywords.h"
#include "mercrender.h"

#define max(a, b)  ((a)>(b)?(a):(b))
#define min(a, b)  ((a)<(b)?(a):(b))

//static char* merc_file     = "/home/gmaps/usnob-images/tycho.mkdt.fits";
//static char* merc_file     = "/h/260/dstn/local/tycho-maps/tycho.mkdt.fits";

static char* merc_template = "/h/260/dstn/raid3/usnob-merctrees/merc_%02i_%02i.mkdt.fits";
// Gridding of Mercator space
static int NM = 32;

static void logmsg(char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "render_usnob: ");
    vfprintf(stderr, format, args);
    va_end(args);
}

int render_usnob(unsigned char* img, render_args_t* args) {
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

    // HACK!
    if (!args->makerawfloatimg && ((xhi - xlo) * (yhi - ylo) > 4)) {
        logmsg("Too many files to read, bailing out.\n");
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

            //snprintf(fn, sizeof(fn), merc_template, i, j);
            snprintf(fn, sizeof(fn), merc_template, j, i);
            logmsg("reading file %s\n", fn);
            merc = merctree_open(fn);
            if (!merc) {
                logmsg("Failed to open merctree %s\n", fn);
                return -1;
            }
            mercrender(merc, args, fluximg);
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

    amp = pow(4.0, min(5, args->zoomlevel)) * 32.0 * exp(args->gain * log(4.0));

    for (j=0; j<args->H; j++) {
        for (i=0; i<args->W; i++) {
            unsigned char* pix;
            double r, g, b, I, f, R, G, B, maxRGB;

            /*
              mercrender() orders the flux as R,B,N. (red,blue,infrared)
              We want
              r = N
              g = R
              b = B
            */

            r = fluximg[3*(j*args->W + i)+2];
            g = fluximg[3*(j*args->W + i)+0];
            b = fluximg[3*(j*args->W + i)+1];

            // color correction?
            //g *= sqrt(args->colorcor);
            //b *= args->colorcor;
		
            I = (r + g + b) / 3;
            if (I == 0.0) {
                R = G = B = 0.0;
            } else {
                if (args->arc) {
                    f = asinh(I * amp);
                } else {
                    f = pow(I * amp, 0.25);
                }
                R = f*r/I;
                G = f*g/I;
                B = f*b/I;
                maxRGB = max(R, max(G, B));
                if (maxRGB > 1.0) {
                    R /= maxRGB;
                    G /= maxRGB;
                    B /= maxRGB;
                }
            }
            pix = pixel(i, j, img, args);

            pix[0] = min(255, 255.0*R);
            pix[1] = min(255, 255.0*G);
            pix[2] = min(255, 255.0*B);
            pix[3] = 255;
        }
    }
    free(fluximg);

    return 0;
}

