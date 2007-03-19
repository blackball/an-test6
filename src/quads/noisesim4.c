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

#include "starutil.h"
#include "mathutil.h"
#include "noise.h"
#include "blind_wcs.h"
#include "sip.h"

const char* OPTIONS = "e:n:ma:u:l:";

#define xy2ra_nowrap(x,y) atan2(y,x)

int main(int argc, char** args) {
	int argchar;

	double ABangle = 4.5; // arcminutes
	double lowerAngle = 4.0;
	double upperAngle = 5.0;
	double pixscale = 0.396; // arcsec / pixel
	//double FWHM = 2 * sqrt(2.0 * log(2.0));
	//double noise = 1.0 / FWHM; // arcsec
	double W = 2048; // field size in pixels
	double H = 1500;
	//double codetol = 0.01;

	// field noise (in pixels)
	double fnoisedist = 1.0;
	// index noise (in radians)
	double inoisedist = arcmin2rad(1.0 / 60.0);

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

	//double realcode[4];
	//double code[4];

	double ra, dec;

	double fieldhyp;

	int j;
	int k;
	int N=1000;

	int matlab = FALSE;

	tan_t tan, ntan;
	double scale, nscale;

	dl* rads;

	while ((argchar = getopt (argc, args, OPTIONS)) != -1)
		switch (argchar) {
			/*
			  case 'e':
			  noise = atof(optarg);
			  dl_append(noises, noise);
			  break;
			*/
		case 'm':
			matlab = TRUE;
			break;
		case 'n':
			N = atoi(optarg);
			break;
		case 'a':
			ABangle = atof(optarg);
			break;
		case 'l':
			lowerAngle = atof(optarg);
			break;
		case 'u':
			upperAngle = atof(optarg);
			break;
		}

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
	for (k=0; k<=20; k++) {
		dl_append(rads, 0.1 * pow(1.2, k));
	}

	for (k=0; k<dl_size(rads); k++) {
		printf("rads(%i)=%g;\n", k+1, dl_get(rads, k));
	}

	// field center.
	ra = 0.0;
	dec = 0.0;
	radec2xyzarr(ra, dec, sCenter);

	for (j=0; j<N; j++) {
		double nab;
		//double ab;
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

		// add noise to the field positions.
		add_field_noise(fA, fnoisedist, nfA);
		add_field_noise(fB, fnoisedist, nfB);
		add_field_noise(fC, fnoisedist, nfC);
		add_field_noise(fD, fnoisedist, nfD);

		// add noise to the star positions.
		add_star_noise(sA, inoisedist, nsA);
		add_star_noise(sB, inoisedist, nsB);
		add_star_noise(sC, inoisedist, nsC);
		add_star_noise(sD, inoisedist, nsD);

		// noisy transform
		blind_wcs_compute_2(nstar, nfield, 4, &ntan, &nscale);

		// true transform
		blind_wcs_compute_2(star, field, 4, &tan, &scale);

		printf("scalef(%i)=%g;\n", j+1, nscale/scale);

		// noisy quad diameter (in arcsec)
		nab = distsq2arcsec(distsq(nsA, nsB, 3));

		// true quad diameter.
		//ab = sqrt(

		// sample stars that are some number of quad radii from the quad center.
		for (k=0; k<dl_size(rads); k++) {
			//int l;
			double nx, ny;
			double s[3];
			double angle = nab / 2.0 * dl_get(rads, k);

			sample_star_in_ring(midAB, angle/60.0, angle/60.0, s);
			xyzarr2radec(s, &ra, &dec);
			ra = rad2deg(ra);
			dec = rad2deg(dec);

			// true projection
			tan_radec2pixelxy(&tan, ra, dec, &x, &y);

			// noisy projection
			tan_radec2pixelxy(&ntan, ra, dec, &nx, &ny);

			printf("d(%i,%i)=%g;\n", j+1, k+1, hypot(x-nx, y-ny));
			printf("dx(%i,%i)=%g;\n", j+1, k+1, x-nx);
			printf("dy(%i,%i)=%g;\n", j+1, k+1, y-ny);
		}
	}

	return 0;
}

// required to link blind_wcs.c
void getstarcoord(uint iA, double *star) {
}
