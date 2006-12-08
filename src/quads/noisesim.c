/**
   This program simulates noise in index star positions and
   computes the resulting noise in the code values that are
   produced.

   See the wiki page:
   -   http://trac.astrometry.net/wiki/ErrorAnalysis
 */
#include <math.h>
#include <stdio.h>

#include "starutil.h"
#include "mathutil.h"
#include "noise.h"

const char* OPTIONS = "e:n:ma:";

int main(int argc, char** args) {
	int argchar;

	double ABangle = 4.5; // arcminutes
	double noise = 3; // arcseconds
	double noisedist;

	double realA[3];
	double realB[3];
	double realC[3];
	double realD[3];
	double realcodecx, realcodecy;
	double realcodedx, realcodedy;

	double A[3];
	double B[3];
	double C[3];
	double D[3];
	double codecx, codecy;
	double codedx, codedy;

	double ra, dec;
	int j;
	int k;
	int N=1000;

	int matlab = FALSE;

	dl* codedelta;
	dl* noises;

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
		}

	// A
	ra = 0.0;
	dec = 0.0;
	radec2xyzarr(ra, dec, realA);

	// B
	ra = deg2rad(ABangle / 60.0);
	dec = 0.0;
	radec2xyzarr(ra, dec, realB);

	if (matlab) {
		printf("realcode=[];\n");
		printf("code    =[];\n");
	}

	printf("noise=[];\n");
	printf("codemean=[];\n");
	printf("codestd=[];\n");

	if (dl_size(noises) == 0)
		dl_append(noises, noise);

	for (k=0; k<dl_size(noises); k++) {

		noise = dl_get(noises, k);
		noisedist = sqrt(arcsec2distsq(noise));

		codedelta = dl_new(256);

		for (j=0; j<N; j++) {
			double midAB[3];

			star_midpoint(midAB, realA, realB);

			// C
			// place C uniformly in the circle around the midpoint of AB.
			sample_star_in_circle(midAB, ABangle, realC);

			// D
			// place D uniformly in the circle around the midpoint of AB.
			sample_star_in_circle(midAB, ABangle, realD);

			{
				double code[4];
				compute_star_code(realA, realB, realC, realD, code);
				realcodecx = code[0];
				realcodecy = code[1];
				realcodedx = code[2];
				realcodedy = code[3];
			}

			// permute A,B,C,D
			add_star_noise(realA, noisedist, A);
			add_star_noise(realB, noisedist, B);
			add_star_noise(realC, noisedist, C);
			add_star_noise(realD, noisedist, D);

			{
				double code[4];
				compute_star_code(A, B, C, D, code);
				codecx = code[0];
				codecy = code[1];
				codedx = code[2];
				codedy = code[3];
			}

			if (matlab) {
				printf("realcode(%i,:)=[%g,%g,%g,%g];\n", j+1,
					   realcodecx, realcodecy, realcodedx, realcodedy);
				printf("code(%i,:)    =[%g,%g,%g,%g];\n", j+1,
					   codecx, codecy, codedx, codedy);
			}

			dl_append(codedelta, codecx - realcodecx);
			dl_append(codedelta, codecy - realcodecy);
			dl_append(codedelta, codedx - realcodedx);
			dl_append(codedelta, codedy - realcodedy);
		}

		{
			double mean, std;
			mean = 0.0;
			for (j=0; j<dl_size(codedelta); j++)
				mean += dl_get(codedelta, j);
			mean /= (double)dl_size(codedelta);
			std = 0.0;
			for (j=0; j<dl_size(codedelta); j++)
				std += square(dl_get(codedelta, j) - mean);
			std /= ((double)dl_size(codedelta) - 1);
			std = sqrt(std);

			printf("noise(%i)=%g; %%arcsec\n", k+1, noise);
			printf("codemean(%i)=%g;\n", k+1, mean);
			printf("codestd(%i)=%g;\n", k+1, std);
		}

		dl_free(codedelta);
	}

	dl_free(noises);
	
	return 0;
}
