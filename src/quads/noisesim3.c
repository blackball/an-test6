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

#define xy2ra_nowrap(x,y) atan2(y,x)

int main(int argc, char** args) {
	int argchar;

	double ABangle = 4.5; // arcminutes
	double lowerAngle = 4.0;
	double upperAngle = 5.0;
	double pixscale = 0.396; // arcsec / pixel
	//double noise = 1; // arcsec
	double FWHM = 2 * sqrt(2.0 * log(2.0));
	double noise = 1.0 / FWHM; // arcsec
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
				double x,y;
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
			{
				double midAB[3];
				double x,y;
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
			}

			// real code:
			compute_star_code(sA, sB, sC, sD, realcode);

			// add noise to the field positions.
			add_field_noise(fA, noisedist, fA);
			add_field_noise(fB, noisedist, fB);
			add_field_noise(fC, noisedist, fC);
			add_field_noise(fD, noisedist, fD);

			// measured code:
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
					double pA[3];
					double pB[3];
					double pC[3];
					double pD[3];
					double pC0[3];
					double pC1[3];
					double pC2[3];
					double pC3[3];
					/*
					  printf("starA(%i,:)=[%g,%g];\n", nhere, 1.0/pixscale * rad2arcsec(xy2ra(sA[0], sA[1])), 1.0/pixscale * rad2arcsec(z2dec(sA[2])));
					  printf("starB(%i,:)=[%g,%g];\n", nhere, 1.0/pixscale * rad2arcsec(xy2ra(sB[0], sB[1])), 1.0/pixscale * rad2arcsec(z2dec(sB[2])));
					  printf("starC(%i,:)=[%g,%g];\n", nhere, 1.0/pixscale * rad2arcsec(xy2ra(sC[0], sC[1])), 1.0/pixscale * rad2arcsec(z2dec(sC[2])));
					  printf("starD(%i,:)=[%g,%g];\n", nhere, 1.0/pixscale * rad2arcsec(xy2ra(sD[0], sD[1])), 1.0/pixscale * rad2arcsec(z2dec(sD[2])));
					  printf("fieldA(%i,:)=[%g,%g];\n", nhere, fA[0], fA[1]);
					  printf("fieldB(%i,:)=[%g,%g];\n", nhere, fB[0], fB[1]);
					  printf("fieldC(%i,:)=[%g,%g];\n", nhere, fC[0], fC[1]);
					  printf("fieldD(%i,:)=[%g,%g];\n", nhere, fD[0], fD[1]);
					*/
					printf("sx(%i,:)=[%g,%g,%g,%g];\n", nhere,
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(sA[0], sA[1])),
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(sB[0], sB[1])),
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(sC[0], sC[1])),
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(sD[0], sD[1])));
					printf("sy(%i,:)=[%g,%g,%g,%g];\n", nhere,
						   1.0/pixscale * rad2arcsec(z2dec(sA[2])),
						   1.0/pixscale * rad2arcsec(z2dec(sB[2])),
						   1.0/pixscale * rad2arcsec(z2dec(sC[2])),
						   1.0/pixscale * rad2arcsec(z2dec(sD[2])));
					printf("fx(%i,:)=[%g,%g,%g,%g];\n", nhere, fA[0], fB[0], fC[0], fD[0]);
					printf("fy(%i,:)=[%g,%g,%g,%g];\n", nhere, fA[1], fB[1], fC[1], fD[1]);
					image_to_xyz(fA[0], fA[1], pA, transform);
					image_to_xyz(fB[0], fB[1], pB, transform);
					image_to_xyz(fC[0], fC[1], pC, transform);
					image_to_xyz(fD[0], fD[1], pD, transform);
					printf("px(%i,:)=[%g,%g,%g,%g];\n", nhere,
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(pA[0], pA[1])),
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(pB[0], pB[1])),
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(pC[0], pC[1])),
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(pD[0], pD[1])));
					printf("py(%i,:)=[%g,%g,%g,%g];\n", nhere,
						   1.0/pixscale * rad2arcsec(z2dec(pA[2])),
						   1.0/pixscale * rad2arcsec(z2dec(pB[2])),
						   1.0/pixscale * rad2arcsec(z2dec(pC[2])),
						   1.0/pixscale * rad2arcsec(z2dec(pD[2])));
					image_to_xyz(0, 0, pC0, transform);
					image_to_xyz(W, 0, pC1, transform);
					image_to_xyz(W, H, pC2, transform);
					image_to_xyz(0, H, pC3, transform);
					printf("cx(%i,:)=[%g,%g,%g,%g,%g];\n", nhere,
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(pC0[0], pC0[1])),
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(pC1[0], pC1[1])),
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(pC2[0], pC2[1])),
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(pC3[0], pC3[1])),
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(pC0[0], pC0[1])));
					printf("cy(%i,:)=[%g,%g,%g,%g,%g];\n", nhere,
						   1.0/pixscale * rad2arcsec(z2dec(pC0[2])),
						   1.0/pixscale * rad2arcsec(z2dec(pC1[2])),
						   1.0/pixscale * rad2arcsec(z2dec(pC2[2])),
						   1.0/pixscale * rad2arcsec(z2dec(pC3[2])),
						   1.0/pixscale * rad2arcsec(z2dec(pC0[2])));
					printf("cenx(%i)=%g;\n", nhere,
						   1.0/pixscale * rad2arcsec(xy2ra_nowrap(pCenter[0], pCenter[1])));
					printf("ceny(%i)=%g;\n", nhere,
						   1.0/pixscale * rad2arcsec(z2dec(pCenter[2])));
					printf("scode(%i,:)=[%g,%g,%g,%g];\n", nhere,
						   realcode[0], realcode[1], realcode[2], realcode[3]);
					printf("fcode(%i,:)=[%g,%g,%g,%g];\n", nhere,
						   code[0], code[1], code[2], code[3]);
					printf("codedist(%i)=%g;\n", nhere, sqrt(distsq(code, realcode, 4)));
					printf("agreedists(%i)=%g;\n", nhere, agree);
					nhere++;
				}
			}
		}

		printf("abinvalid=%g\n", (double)abInvalid / (double)N);
		printf("cdinvalid=%g\n", (double)cdInvalid / (double)N);
		printf("codeinvalid=%g\n", (double)codeInvalid / (double)N);
		printf("ok=%g\n", (double)dl_size(agreedists) / (double)N);

		printf("I=173; plot(sx(I,:), sy(I,:),'ro-', px(I,:), py(I,:), 'bx--', cx(I,:), cy(I,:), 'k--', cenx(I), ceny(I), 'mx', 0, 0, 'ro'); axis equal;\n");
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
			printf("agreemean(%i)=%g;\n", k+1, mean);
			printf("agreestd(%i)=%g;\n", k+1, std);
		}

		dl_free(agreedists);
	}

	dl_free(noises);
	return 0;
}
