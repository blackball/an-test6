#include <stdio.h>
#include <math.h>

#include "tilerender.h"
#include "render_rdls.h"
#include "rdlist.h"
#include "starutil.h"
#include "mathutil.h"
#include "mercrender.h"

char* rdls_dirs[] = {
	"/home/gmaps/gmaps-rdls/",
	"/home/gmaps/ontheweb-data/"
};

int render_rdls(unsigned char* img, render_args_t* args)
{
	int i;
	xylist* rdls;

	/* Search in the rdls paths */
	for (i=0; i<sizeof(rdls_dirs)/sizeof(char*); i++) {
		char fn[256];
		snprintf(fn, sizeof(fn), "%s/%s", rdls_dirs[i], args->rdlsfn);
		fprintf(stderr, "Trying file: %s\n", fn);
		rdls = rdlist_open(fn);
		if (rdls)
			break;
	}
	if (!rdls) {
		fprintf(stderr, "Failed to open RDLS file.\n");
		return -1;
	}

	int Nstars = rdlist_n_entries(rdls, args->fieldnum);
	if (Nstars == -1) {
		fprintf(stderr, "Failed to read RDLS file.\n");
		exit(-1);
	}

	if (args->Nstars && args->Nstars < Nstars)
		Nstars = args->Nstars;

	double* rdvals = malloc(Nstars * 2 * sizeof(double));
	if (rdlist_read_entries(rdls, args->fieldnum, 0, Nstars, rdvals)) {
		fprintf(stderr, "Failed to read RDLS file.\n");
		free(rdvals);
		return -1;
	}

	fprintf(stderr, "Got %i stars.\n", Nstars);

	float* fluximg = calloc(args->W*args->H*3, sizeof(float));
	if (!fluximg) {
		fprintf(stderr, "Failed to allocate flux image.\n");
		exit(-1);
	}

	int Nib = 0;
	for (i=0; i<Nstars; i++) {
		double px = ra2pixel(rdvals[i*2+0], args);
		double py = dec2pixel(rdvals[i*2+1], args);

		/* This is just a hack by using 255.0.... */
		Nib += add_star(px, py, 255.0, 255.0, 255.0, fluximg, args);
	}

	fprintf(stderr, "%i stars inside image bounds.\n", Nib);

	int j;
	for (j=0; i<args->H; i++) {
		for (i=0; i<args->W; i++) {
			unsigned char* pix = pixel(i,j, img, args);
			pix[0] = min(fluximg[3*(j*args->W+i)+0], 255);
			pix[1] = min(fluximg[3*(j*args->W+i)+1], 255);
			pix[2] = min(fluximg[3*(j*args->W+i)+2], 255);
			fwrite(pix, 1, 3, stdout);
		}
	}

	free(fluximg);
	free(rdvals);

	return 0;
}
