#include <stdio.h>
#include <math.h>

#include "tilerender.h"
#include "render_gridlines.h"

int render_gridlines(unsigned char* img, render_args_t* args) {
	double rastep, decstep;
	int ind;
	double steps[] = { 1, 10.0, 5.0, 2.0, 1.0, 0.5, 0.25, 5.0/60.0,
					   2.0/60.0, 1.0/60.0, 0.5/60.0, 10/3600.0 };
	double ra, dec;
	int i;

	ind = max(1, args->zoomlevel);
	ind = min(ind, sizeof(steps)/sizeof(double)-1);
	rastep = decstep = steps[ind];

	for (ra = rastep * floor(args->ramin / rastep);
		 ra <= rastep * ceil(args->ramax / rastep);
		 ra += rastep) {
		int x = ra2pixel(ra, args);
		if (!in_image(x, 0, args))
			continue;
		for (i=0; i<args->H; i++) {
			uchar* pix = pixel(x, i, img, args);
			pix[0] = 50;
			pix[1] = 50;
			pix[2] = 255;
			pix[3] = 128;
		}
	}
	for (dec = decstep * floor(args->decmin / decstep);
		 dec <= decstep * ceil(args->decmax / decstep);
		 dec += decstep) {
		int y = dec2pixel(dec, args);
		if (!in_image(0, y, args))
			continue;
		for (i=0; i<args->W; i++) {
			uchar* pix = pixel(i, y, img, args);
			pix[0] = 50;
			pix[1] = 50;
			pix[2] = 255;
			pix[3] = 128;
		}
	}

	return 0;
}
