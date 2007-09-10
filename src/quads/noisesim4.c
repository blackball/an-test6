/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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

/**
   This program tries to determine how error in field+index
   stars propagates to errors in the rigid TAN transformation.

   We place a quad on the sky to get ground-truth index locations,
   then project to get ground-truth field locations.  From this we
   compute the true transformation.  Next we add index noise and field
   noise, and compute the noisy transformation.

   We then try to decompose the transformation noise into scaling, rotation
   and translation errors, and measure their magnitudes.
 */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "starutil.h"
#include "mathutil.h"
#include "noise.h"
#include "blind_wcs.h"
#include "sip.h"

const char* OPTIONS = "e:n:ma:H";

#define xy2ra_nowrap(x,y) atan2(y,x)

int main(int argc, char** args) {
	int argchar;

	double ABangle = 4.5; // arcminutes
	double pixscale = 0.396; // arcsec / pixel

	double W = 2048; // field size in pixels
	double H = 1500;

	// field noise (in pixels)
	double fnoise = 1.0;

	// index noise (arcseconds)
	double inoise_arcsec = 1.0;
	// index noise (in radians)
	double inoise;

	double sCenter[3];
	// star coords
	double star[12];
	double *sA, *sB, *sC, *sD;
	// noisy
	double nstar[12];
	double *nsA, *nsB, *nsC, *nsD;

	// field coords
	double field[8];
	double *fA, *fB, *fC, *fD;
	// noisy
	double nfield[8];
	double *nfA, *nfB, *nfC, *nfD;

	double ra, dec;
	double fieldhyp;

	int j;
	int k;
	int N=1000;

	int matlab = FALSE;

	tan_t tan, ntan;
	double scale, nscale;

	int hyp = FALSE;

	dl* rads;

	double *dx, *dy;

	while ((argchar = getopt (argc, args, OPTIONS)) != -1)
		switch (argchar) {
		case 'H':
			hyp = TRUE;
			break;
		case 'e':
			fnoise = atof(optarg);
			break;
		case 'm':
			matlab = TRUE;
			break;
		case 'n':
			N = atoi(optarg);
			break;
		case 'a':
			ABangle = atof(optarg);
			break;
		}

	srand(time(NULL));

	inoise = arcmin2rad(inoise_arcsec / 60.0);

	printf("fnoise = %g;\n", fnoise);
	printf("inoise = %g;\n", rad2arcsec(inoise) / pixscale);
	
	sA = star;
	sB = star + 3;
	sC = star + 6;
	sD = star + 9;
	nsA = nstar;
	nsB = nstar + 3;
	nsC = nstar + 6;
	nsD = nstar + 9;
	fA = field;
	fB = field + 2;
	fC = field + 4;
	fD = field + 6;
	nfA = nfield;
	nfB = nfield + 2;
	nfC = nfield + 4;
	nfD = nfield + 6;

	rads = dl_new(16);
	/*
	  dl_append(rads, 0.0);
	  dl_append(rads, 0.5);
	  dl_append(rads, 1.0);
	  dl_append(rads, 2.0);
	  dl_append(rads, 4.0);
	  dl_append(rads, 8.0);
	*/
	/*
	  for (k=0; k<=100; k++) {
	  dl_append(rads, 0.1 * k);
	  }
	*/
	/*
	  for (k=0; k<=20; k++) {
	  dl_append(rads, k==0 ? 0 : (0.1 * pow(1.2, k-1)));
	  }
	*/
	for (k=0; k<=50; k++) {
		dl_append(rads, k==0 ? 0 : (0.1 * pow(1.1, k-1)));
	}

	for (k=0; k<dl_size(rads); k++) {
		printf("rads(%i)=%g;\n", k+1, dl_get(rads, k));
	}

	dx = calloc(N*dl_size(rads), sizeof(double));
	dy = calloc(N*dl_size(rads), sizeof(double));

	// field center.
	ra = 0.0;
	dec = 0.0;
	radec2xyzarr(ra, dec, sCenter);

	for (j=0; j<N; j++) {
		double nab;
		double midAB[3];
		double x,y;
		// sample A in the field's bounding circle
		fieldhyp = hypot(W, H);
		for (;;) {
			sample_star_in_circle(sCenter, fieldhyp/2.0 * pixscale / 60.0, sA);
			star_coords(sA, sCenter, &x, &y);
			// radian-like units
			x = rad2arcsec(x) / pixscale + W/2.0;
			y = rad2arcsec(y) / pixscale + H/2.0;
			// reject stars not in the field
			if (x < 0 || x > W || y < 0 || y > H)
				continue;
			fA[0] = x;
			fA[1] = y;
			break;
		}

		// sample B in a circle around A.
		for (;;) {
			sample_star_in_ring(sA, ABangle, ABangle, sB);
			star_coords(sB, sCenter, &x, &y);
			x = rad2arcsec(x) / pixscale + W/2.0;
			y = rad2arcsec(y) / pixscale + H/2.0;
			if (x < 0 || x > W || y < 0 || y > H)
				continue;
			fB[0] = x;
			fB[1] = y;
			break;
		}

		// sample C,D centered at midAB.
		star_midpoint(midAB, sA, sB);
		for (;;) {
			sample_star_in_circle(midAB, ABangle/2.0, sC);
			star_coords(sC, sCenter, &x, &y);
			x = rad2arcsec(x) / pixscale + W/2.0;
			y = rad2arcsec(y) / pixscale + H/2.0;
			if (x < 0 || x > W || y < 0 || y > H)
				continue;
			fC[0] = x;
			fC[1] = y;
			break;
		}
		for (;;) {
			sample_star_in_circle(midAB, ABangle/2.0, sD);
			star_coords(sD, sCenter, &x, &y);
			x = rad2arcsec(x) / pixscale + W/2.0;
			y = rad2arcsec(y) / pixscale + H/2.0;
			if (x < 0 || x > W || y < 0 || y > H)
				continue;
			fD[0] = x;
			fD[1] = y;
			break;
		}

		if (hyp) {
			// effective field noise
			double effnoise = hypot(fnoise, rad2arcsec(inoise) / pixscale);
			fnoise = effnoise;
			inoise = 0.0;
		/*
			// add noise to the field positions.
			add_field_noise(fA, effnoise, nfA);
			add_field_noise(fB, effnoise, nfB);
			add_field_noise(fC, effnoise, nfC);
			add_field_noise(fD, effnoise, nfD);

			// add (zero) noise to the star positions.
			add_star_noise(sA, 0, nsA);
			add_star_noise(sB, 0, nsB);
			add_star_noise(sC, 0, nsC);
			add_star_noise(sD, 0, nsD);
		*/
		}


		// add noise to the field positions.
		add_field_noise(fA, fnoise, nfA);
		add_field_noise(fB, fnoise, nfB);
		add_field_noise(fC, fnoise, nfC);
		add_field_noise(fD, fnoise, nfD);

		// add noise to the star positions.
		add_star_noise(sA, inoise, nsA);
		add_star_noise(sB, inoise, nsB);
		add_star_noise(sC, inoise, nsC);
		add_star_noise(sD, inoise, nsD);

		// noisy transform
		blind_wcs_compute(nstar, nfield, 4, &ntan, &nscale);

		// true transform
		blind_wcs_compute(star, field, 4, &tan, &scale);

		//printf("scalef(%i)=%g;\n", j+1, nscale/scale);

		// noisy quad diameter (in arcsec)
		nab = distsq2arcsec(distsq(nsA, nsB, 3));

		// sample stars that are some number of quad radii from the quad center.
		for (k=0; k<dl_size(rads); k++) {
			double nx, ny;
			double s[3];
			double angle = nab / 2.0 * dl_get(rads, k);
			double xy[2], nxy[2];

			sample_star_in_ring(midAB, angle/60.0, angle/60.0, s);
			xyzarr2radec(s, &ra, &dec);
			ra = rad2deg(ra);
			dec = rad2deg(dec);

			// true projection
			tan_radec2pixelxy(&tan, ra, dec, &x, &y);

			// noisy projection
			tan_radec2pixelxy(&ntan, ra, dec, &nx, &ny);

			// add field noise
			xy[0] = nx;
			xy[1] = ny;
			add_field_noise(xy, fnoise, nxy);
			nx = nxy[0];
			ny = nxy[1];

			/*
			  printf("d(%i,%i)=%g;\n", j+1, k+1, hypot(x-nx, y-ny));
			  printf("dx(%i,%i)=%g;\n", j+1, k+1, x-nx);
			  printf("dy(%i,%i)=%g;\n", j+1, k+1, y-ny);
			*/
			/*
			  dx[j*dl_size(rads) + k] = x-nx;
			  dy[j*dl_size(rads) + k] = y-ny;
			*/
			printf("d(%i,%i)=%g;\n", j+1, k+1, hypot(x-nx, y-ny));

			//printf("dx(%i,%i) = %g;\n", 
		}
	}

/*
	{
		double meandx, meandy;
		double stdx, stdy;
		int K = dl_size(rads);
		printf("sx=[];\n");
		printf("sy=[];\n");
		for (k=0; k<K; k++) {
			meandx = meandy = 0;
			for (j=0; j<N; j++) {
				meandx += dx[j*K + k];
				meandy += dy[j*K + k];
			}
			meandx /= (double)N;
			meandy /= (double)N;
			stdx = stdy = 0;
			for (j=0; j<N; j++) {
				stdx += (dx[j*K+k]-meandx)*(dx[j*K+k]-meandx);
				stdy += (dy[j*K+k]-meandy)*(dy[j*K+k]-meandy);
			}
			stdx /= (double)N;
			stdy /= (double)N;
			stdx = sqrt(stdx);
			stdy = sqrt(stdy);
			printf("sx(%i)=%g;\n", k+1, stdx);
			printf("sy(%i)=%g;\n", k+1, stdy);
		}
	}
*/
	return 0;
}

// required to link blind_wcs.c
void getstarcoord(uint iA, double *star) {
}
