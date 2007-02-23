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
		fprintf(stderr, "render_rdls: Trying file: %s\n", fn);
		rdls = rdlist_open(fn);
		if (rdls)
			break;
	}
	if (!rdls) {
		fprintf(stderr, "render_rdls: Failed to open RDLS file.\n");
		return -1;
	}

	int Nstars = rdlist_n_entries(rdls, args->fieldnum);
	if (Nstars == -1) {
		fprintf(stderr, "render_rdls: Failed to read RDLS file.\n");
		exit(-1);
	}

	if (args->Nstars && args->Nstars < Nstars)
		Nstars = args->Nstars;

	double* rdvals = malloc(Nstars * 2 * sizeof(double));
	if (rdlist_read_entries(rdls, args->fieldnum, 0, Nstars, rdvals)) {
		fprintf(stderr, "render_rdls: Failed to read RDLS file.\n");
		free(rdvals);
		return -1;
	}

	fprintf(stderr, "render_rdls: Got %i stars.\n", Nstars);

	float* fluximg = calloc(args->W*args->H*3, sizeof(float));
	if (!fluximg) {
		fprintf(stderr, "render_rdls: Failed to allocate flux image.\n");
		exit(-1);
	}

	int Nib = 0;
	for (i=0; i<Nstars; i++) {
		double px =  ra2merc(deg2rad(rdvals[i*2+0]));
		double py = dec2merc(deg2rad(rdvals[i*2+1]));

		/* This is just a hack by using 255.0.... */
		//fprintf(stderr, "render_rdls: %g %g %d\n", px, py, Nib);
		Nib += add_star(px, py, 205.0, 355.0, 0.0, fluximg, args);
	}

	fprintf(stderr, "render_rdls: %i stars inside image bounds.\n", Nib);

	int j;
	for (j=0; j<args->H; j++) {
		for (i=0; i<args->W; i++) {
			unsigned char* pix = pixel(i,j, img, args);
			pix[0] = min(fluximg[3*(j*args->W+i)+0], 255);
			pix[1] = min(fluximg[3*(j*args->W+i)+1], 255);
			pix[2] = min(fluximg[3*(j*args->W+i)+2], 255);
			//pix[3] = (pix[0]+pix[1]+pix[2])/3.0;
			pix[3] = 0;
			// hack; adds a black 'outline' 
			if (pix[0] || pix[1] || pix[2]) {
				pix[3] = 255;
			}
		}
	}

	free(fluximg);
	free(rdvals);

	return 0;
}

static int rdls_add_star(float* fluximg, int x, int y, int W, int H) {
	// X
	/*
	  int dx[] = {  0, -1, -2,  1,  2,  1,  2, -1, -2,
	  1,  0, -1,  2,  3,  2,  3,  0, -1 };
	  int dy[] = {  0, -1, -2,  1,  2, -1, -2,  1,  2,
	  0, -1, -2,  1,  2, -1, -2,  1,  2 };
	*/
	// O
	int dx[] = { -1,  0,  1,  2, -1,  0,  1,  2,
				 -2, -2, -2, -2,  3,  3,  3,  3 };
	int dy[] = { -2, -2, -2, -2,  3,  3,  3,  3,
				 -1,  0,  1,  2, -1,  0,  1,  2 };

	float rflux, gflux, bflux;
	int i;
	/*
		rflux = 255.0;
		gflux = 200.0;
		bflux = 0.0;
	*/
	rflux = 255.0;
	gflux = 255.0;
	bflux = 255.0;

	for (i=0; i<sizeof(dx)/sizeof(int); i++) {
		if ((x + dx[i] < 0) || (x + dx[i] >= W)) continue;
		if ((y + dy[i] < 0) || (y + dy[i] >= H)) continue;
		fluximg[3*((y+dy[i])*W+(x+dx[i])) + 0] += rflux;
		fluximg[3*((y+dy[i])*W+(x+dx[i])) + 1] += gflux;
		fluximg[3*((y+dy[i])*W+(x+dx[i])) + 2] += bflux;
	}
}

