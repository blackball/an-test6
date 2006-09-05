/*
*/

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>

#include "pm.h"
#include "ppm.h"
#include "pnm.h"

int
main(int argc, char** args) {
    FILE *fin;
	char* fn;
    int rows, cols, format;
	pixval rmax, gmax, bmax, maxval;
    int row;
    pixel * pixelrow;
	FILE *fout;
	off_t imgstart;
	pixval *rmap, *bmap, *gmap;
	int i;

    pnm_init(&argc, args);

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <input-ppm>\n", args[0]);
		exit(-1);
	}
	fn = args[1];

    fin = pm_openr_seekable(fn);

    ppm_readppminit(fin, &cols, &rows, &maxval, &format);

	if (PNM_FORMAT_TYPE(format) != PPM_TYPE) {
		fprintf(stderr, "Input file must be PPM.\n");
		exit(-1);
	}

    pixelrow = ppm_allocrow(cols);

	rmap = malloc(maxval * sizeof(pixval));
	gmap = malloc(maxval * sizeof(pixval));
	bmap = malloc(maxval * sizeof(pixval));
	if (!rmap || !gmap || !bmap) {
		fprintf(stderr, "Failed to allocate color maps.\n");
		exit(-1);
	}

	imgstart = ftello(fin);

	rmax = gmax = bmax = 0;
    for (row = 0; row < rows; ++row) {
        int col;
        ppm_readppmrow(fin, pixelrow, cols, maxval, format);
        for (col = 0; col < cols; ++col) {
            pixel const p = pixelrow[col];
			if (p.r > rmax) rmax = p.r;
			if (p.g > gmax) gmax = p.g;
			if (p.b > bmax) bmax = p.b;
		}
	}

	fout = stdout;
	ppm_writeppminit(fout, cols, rows, maxval, 0);

	fseeko(fin, imgstart, SEEK_SET);

	for (i=0; i<maxval; i++) {
		rmap[i] = (pixval)rint((double)i * maxval / (double)rmax);
		gmap[i] = (pixval)rint((double)i * maxval / (double)gmax);
		bmap[i] = (pixval)rint((double)i * maxval / (double)bmax);
	}

    for (row = 0; row < rows; ++row) {
        int col;
        ppm_readppmrow(fin, pixelrow, cols, maxval, format);
        for (col = 0; col < cols; ++col) {
            pixel p = pixelrow[col];
			p.r = rmap[p.r];
			p.g = gmap[p.g];
			p.b = bmap[p.b];
			pixelrow[col] = p;
		}
		ppm_writeppmrow(fout, pixelrow, cols, maxval, 0);
	}
	
    pnm_freerow(pixelrow);

    pm_close(fin);
    return 0;
}
