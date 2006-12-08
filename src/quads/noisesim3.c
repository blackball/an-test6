/**
   This program tries to determine how to set the agreement
   threshold.  We want to know, given known noise in the
   field, how far does the estimated center of the field
   (as computed from a match) move from the true position.


   We do this by first placing a quad on the sky, then
   projecting the stars and adding field noise to get field
   positions.  Assuming the quad is still valid, and that it
   passes the code tolerance, we measure the distance between
   the estimated field center and the true field center.

   (We'll assume that the index is ground-truth.)

	// -place field center at 0,0
	// -sample star A centered at the field center, with radius big enough to
	//  contain the field.
	//    -reject stars outside the field.
	// -sample star B centered at A
	//    -reject those with incorrect scale or outside the field.
	// -sample stars C,D centered at midAB.
	//    -reject those outside the field.

	// -project A,B,C,D around the field center.
	// -add field noise.
	// -compute the code.
	// -reject invalid codes.
	// -check code tolerance.

	// -compute transformation matrix
	// -transform field center
	// -record distance from 0,0.

   See the wiki page:
   -   http://trac.astrometry.net/wiki/ErrorAnalysis
 */
#include <math.h>
#include <stdio.h>

#include "starutil.h"
#include "mathutil.h"
#include "noise.h"

const char* OPTIONS = "e:n:ma:u:l:";

int main(int argc, char** args) {
	int argchar;

	double ABangle = 4.5; // arcminutes
	double lowerAngle = 4.0;
	double upperAngle = 5.0;
	double pixscale = 0.396; // arcsec / pixel
	double noise = 3; // arcsec
	double W = 2048; // field size in pixels
	double H = 1500;
	double codetol = 0.01;

	double noisedist;

	// star coords
	double sCenter[3];
	double sA[3], sB[3], sC[3], sD[3];

	// field coords
	double fA[2], fB[2], fC[2], fD[2];

	double realcode[4];
	double code[4];

	double ra, dec;

	double fieldhyp;

	int j;
	int k;
	int N=1000;

	int matlab = FALSE;

	dl* noises;
	dl* agreedists;

	int abInvalid;
	int cdInvalid;
	int codeInvalid;

	noises = dl_new(16);

	while ((argchar = getopt (argc, args, OPTIONS)) != -1)
		switch (argchar) {
		case 'e':
			noise = atof(optarg);
			dl_append(noises, noise);
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
		case 'l':
			lowerAngle = atof(optarg);
			break;
		case 'u':
			upperAngle = atof(optarg);
			break;
		}

	// field center.
	ra = 0.0;
	dec = 0.0;
	radec2xyzarr(ra, dec, sCenter);

	if (dl_size(noises) == 0)
		dl_append(noises, noise);

	for (k=0; k<dl_size(noises); k++) {
		noise = dl_get(noises, k);
		noisedist = noise / pixscale;

		agreedists = dl_new(256);

		abInvalid = cdInvalid = codeInvalid = 0;

		if (matlab)
			printf("agreedists=[];\n");

		for (j=0; j<N; j++) {

			// sample A in the field's bounding circle
			fieldhyp = hypot(W, H);
			for (;;) {
				double x,y;
				sample_star_in_circle(sCenter, fieldhyp/2.0 * pixscale / 60.0, sA);
				// reject stars not in the field
				star_coords(sA, sCenter, &x, &y);
				// radian-like units
				x = rad2arcsec(x) / pixscale + W/2.0;
				y = rad2arcsec(y) / pixscale + H/2.0;
				if (x < 0 || x > W || y < 0 || y > H)
					continue;
				fA[0] = x;
				fA[1] = y;
				break;
			}

			// sample B in a ring around A.
			for (;;) {
				double x,y;
				sample_star_in_ring(sA, ABangle, ABangle, sB);
				// reject stars not in the field
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
			{
				double midAB[3];
				double x,y;
				star_midpoint(midAB, sA, sB);
				for (;;) {
					sample_star_in_circle(midAB, ABangle/2.0, sC);
					// reject stars not in the field
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
					// reject stars not in the field
					star_coords(sD, sCenter, &x, &y);
					x = rad2arcsec(x) / pixscale + W/2.0;
					y = rad2arcsec(y) / pixscale + H/2.0;
					if (x < 0 || x > W || y < 0 || y > H)
						continue;
					fD[0] = x;
					fD[1] = y;
					break;
				}
			}

			// add noise to the field positions.
			add_field_noise(fA, noisedist, fA);
			add_field_noise(fB, noisedist, fB);
			add_field_noise(fC, noisedist, fC);
			add_field_noise(fD, noisedist, fD);

			// codes
			compute_star_code(sA, sB, sC, sD, realcode);
			{
				double scale;
				compute_field_code(fA, fB, fC, fD, code, &scale);

				if ((scale < square(lowerAngle * 60.0 / pixscale)) ||
					(scale > square(upperAngle * 60.0 / pixscale))) {
					abInvalid++;
					continue;
				}

				if ((((code[0]*code[0] - code[0]) + (code[1]*code[1] - code[1])) > 0.0) ||
					(((code[2]*code[2] - code[2]) + (code[3]*code[3] - code[3])) > 0.0)) {
					cdInvalid++;
					continue;
				}
			}

			// check code tolerance.
			if (distsq(code, realcode, 4) > codetol*codetol) {
				codeInvalid++;
				continue;
			}

			{
				double starcoords[12];
				double fieldcoords[8];
				double transform[9];
				double pCenter[3];
				double agree;

				memcpy(starcoords + 0, sA, 3*sizeof(double));
				memcpy(starcoords + 3, sB, 3*sizeof(double));
				memcpy(starcoords + 6, sC, 3*sizeof(double));
				memcpy(starcoords + 9, sD, 3*sizeof(double));
				memcpy(fieldcoords + 0, fA, 2*sizeof(double));
				memcpy(fieldcoords + 2, fB, 2*sizeof(double));
				memcpy(fieldcoords + 4, fC, 2*sizeof(double));
				memcpy(fieldcoords + 6, fD, 2*sizeof(double));

				// compute transformation
				fit_transform(starcoords, fieldcoords, 4, transform);

				// project field center.
				image_to_xyz(W/2.0, H/2.0, pCenter, transform);

				// compute distance from true field center (in arcseconds).
				agree = distsq2arcsec(distsq(pCenter, sCenter, 3));

				dl_append(agreedists, agree);

				if (matlab) {
					static int nhere = 1;
					printf("agreedists(%i)=%g;\n", nhere, agree);
					nhere++;
				}
			}
		}

		printf("abinvalid=%g\n", (double)abInvalid / (double)N);
		printf("cdinvalid=%g\n", (double)cdInvalid / (double)N);
		printf("codeinvalid=%g\n", (double)codeInvalid / (double)N);
		printf("ok=%g\n", (double)dl_size(agreedists) / (double)N);

		{
			double mean, std;
			mean = 0.0;
			for (j=0; j<dl_size(agreedists); j++)
				mean += dl_get(agreedists, j);
			mean /= (double)dl_size(agreedists);
			std = 0.0;
			for (j=0; j<dl_size(agreedists); j++)
				std += square(dl_get(agreedists, j) - mean);
			std /= ((double)dl_size(agreedists) - 1);
			std = sqrt(std);

			printf("noise(%i)=%g; %%arcsec\n", k+1, noise);
			printf("agreestd(%i)=%g;\n", k+1, std);
		}

		dl_free(agreedists);
	}

	dl_free(noises);
	return 0;
}
