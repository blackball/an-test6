#include <stdio.h>
#include <math.h>

#include "tilerender.h"
#include "render_tycho.h"
#include "starutil.h"
#include "mathutil.h"
#include "merctree.h"
#include "keywords.h"
#include "mercrender.h"

#define max(a, b)  ((a)>(b)?(a):(b))
#define min(a, b)  ((a)<(b)?(a):(b))

static char* merc_file     = "/home/gmaps/usnob-images/tycho.mkdt.fits";
//static char* merc_file     = "/h/260/dstn/local/tycho-maps/tycho.mkdt.fits";

int render_tycho(unsigned char* img, render_args_t* args) {
	float* fluximg;
	float amp = 0.0;
	int i, j;

	fprintf(stderr, "hello world\n");

	fluximg = mercrender_file(merc_file, args);
	if (!fluximg) {
		return -1;
	}

	amp = pow(4.0, min(5, args->zoomlevel)) * 32.0 * exp(args->gain * log(4.0));

	for (j=0; j<args->H; j++) {
		for (i=0; i<args->W; i++) {
			unsigned char* pix;
			double r, g, b, I, f, R, G, B, maxRGB;

			r = fluximg[3*(j*args->W + i)+0];
			b = fluximg[3*(j*args->W + i)+1];

			if (args->arith)
				g = (r + b) / 2.0;
			else
				g = sqrt(r * b);

			// color correction
			g *= sqrt(args->colorcor);
			b *= args->colorcor;
		
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

