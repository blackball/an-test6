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
   This program simulates noise in index star positions and
   computes the resulting noise in the code values that are
   produced.

   See the wiki page:
   -   http://trac.astrometry.net/wiki/ErrorAnalysis
 */
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include "starutil.h"
#include "mathutil.h"
#include "noise.h"

const char* OPTIONS = "e:n:ma:ri:s:";

int main(int argc, char** args) {
	int argchar;

    //double fwhm = 1.0 / (2.0 * sqrt(2.0 * log(2.0)));

	double ABangle = 4.0; // arcminutes
	double pixscale = 0.396; // arcsec/pixel
	//	double pixscale = 0.05; // Hubble
	double pixnoise = 1; // pixels, stddev
	double indexjitter = 1.0; // arcsec, stddev
	double noisedist;

	double realA[3];
	double realB[3];
	double realC[3];
	double realD[3];

	double A[3];
	double B[3];
	double C[3];
	double D[3];

	double ra, dec;
	int j;
	int k;
	int N=1000;

	int matlab = FALSE;

	dl* codedelta;
	dl* codedists;
	dl* noises;

    int real = FALSE;

	noises = dl_new(16);

    srand((unsigned int)time(NULL));

	while ((argchar = getopt (argc, args, OPTIONS)) != -1)
		switch (argchar) {
        case 'i':
            indexjitter = atof(optarg);
            break;
		case 'e':
			pixnoise = atof(optarg);
			dl_append(noises, pixnoise);
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
        case 'r':
            real = TRUE;
            break;
        case 's':
            pixscale = atof(optarg);
            break;
		}

	// A
	ra = 0.0;
	dec = 0.0;
	radec2xyzarr(ra, dec, realA);

	// B
	ra = arcmin2rad(ABangle);
	dec = 0.0;
	radec2xyzarr(ra, dec, realB);

	if (matlab && real) {
		printf("icode=[];\n");
		printf("fcode=[];\n");
	}

    printf("pixscale=%g;\n", pixscale);
 	printf("quadsize=%g;\n", ABangle);
	printf("codemean=[];\n");
	printf("codestd=[];\n");
	printf("pixerrs=[];\n");
    printf("indexnoise=%g;\n", indexjitter);

	if (dl_size(noises) == 0)
		dl_append(noises, pixnoise);

	for (k=0; k<dl_size(noises); k++) {
        double mean, std;

		pixnoise = dl_get(noises, k);
		noisedist = arcsec2dist(indexjitter);

		codedelta = dl_new(256);
		codedists = dl_new(256);

		for (j=0; j<N; j++) {
			double midAB[3];
			double fcode[4];
			double icode[4];
			double field[8];
			int i;
            bool ok = TRUE;

			star_midpoint(midAB, realA, realB);

			// C
			// place C uniformly in the circle around the midpoint of AB.
			sample_star_in_circle(midAB, ABangle/2.0, realC);

			// D
			// place D uniformly in the circle around the midpoint of AB.
			sample_star_in_circle(midAB, ABangle/2.0, realD);

			// add noise to A,B,C,D
			add_star_noise(realA, noisedist, A);
			add_star_noise(realB, noisedist, B);
			add_star_noise(realC, noisedist, C);
			add_star_noise(realD, noisedist, D);

			compute_star_code(A, B, C, D, icode);

			//compute_star_code(realA, realB, realC, realD, realcode);

			// project to field coords
			ok &= star_coords(realA, midAB, field+0, field+1);
			ok &= star_coords(realB, midAB, field+2, field+3);
			ok &= star_coords(realC, midAB, field+4, field+5);
			ok &= star_coords(realD, midAB, field+6, field+7);
            assert(ok);
			// scale to pixels.
			for (i=0; i<8; i++)
				field[i] = rad2arcsec(field[i]) / pixscale;

			// add field noise
			add_field_noise(field+0, pixnoise, field+0);
			add_field_noise(field+2, pixnoise, field+2);
			add_field_noise(field+4, pixnoise, field+4);
			add_field_noise(field+6, pixnoise, field+6);

			compute_field_code(field+0, field+2, field+4, field+6, fcode, NULL);

            if (matlab && real) {
                //double ra = atan2(A[1],A[0]);
                //printf("A(%i,:)=[%g,%g];\n", j+1, rad2arcsec(ra), rad2arcsec(z2dec(A[2])));
                printf("icode(%i,:)=[%g,%g,%g,%g];\n", j+1,
                       icode[0], icode[1], icode[2], icode[3]);
                printf("fcode(%i,:)=[%g,%g,%g,%g];\n", j+1,
                       fcode[0], fcode[1], fcode[2], fcode[3]);
            }

			/*
			  for (i=0; i<4; i++)
			  dl_append(codedelta, code[i] - realcode[i]);
			*/

			dl_append(codedists, sqrt(distsq(icode, fcode, 4)));
		}

        if (matlab) {
            printf("codedists%i=[", k+1);
            for (j=0; j<dl_size(codedists); j++)
                printf("%g,", dl_get(codedists, j));
            printf("];\n");
        }

        mean = 0.0;
        for (j=0; j<dl_size(codedists); j++)
            mean += dl_get(codedists, j);
        mean /= (double)dl_size(codedists);
        std = 0.0;
        for (j=0; j<dl_size(codedists); j++)
            std += square(dl_get(codedists, j) - mean);
        std /= ((double)dl_size(codedists) - 1);
        std = sqrt(std);

		printf("pixerrs(%i)=%g;\n", k+1, pixnoise);
        printf("codemean(%i)=%g;\n", k+1, mean);
        printf("codestd(%i)=%g;\n", k+1, std);

		dl_free(codedelta);
		dl_free(codedists);
	}

	dl_free(noises);
	
	return 0;
}
