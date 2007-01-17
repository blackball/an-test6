/**
   This program simulates noise in field object positions and
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

const char* OPTIONS = "e:n:ma:u:l:c";

int main(int argc, char** args) {
	int argchar;

	double ABangle = 4.5; // arcminutes
	double pixscale = 0.396; // arcsec / pixel
	double noise = 3; // arcsec

	double lowerAngle = 4.0;
	double upperAngle = 5.0;

	double noisedist;

	double realA[2];
	double realB[2];
	double realC[2];
	double realD[2];
	double realcodecx, realcodecy;
	double realcodedx, realcodedy;

	double A[2];
	double B[2];
	double C[2];
	double D[2];
	double codecx, codecy;
	double codedx, codedy;

	int j;
	int k;
	int N=1000;

	int matlab = FALSE;
	int print_codedists = FALSE;

	dl* codedelta;
	dl* codedists;
	dl* noises;

	int abInvalid = 0;
	int cdInvalid = 0;

	noises = dl_new(16);

	while ((argchar = getopt (argc, args, OPTIONS)) != -1)
		switch (argchar) {
		case 'e':
			noise = atof(optarg);
			dl_append(noises, noise);
			break;
		case 'c':
			print_codedists = TRUE;
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

	// A
	realA[0] = realA[1] = 0.0;

	// B
	realB[0] = 0.0;
	realB[1] = ABangle * 60.0 / pixscale;

	if (matlab) {
		printf("realcode=[];\n");
		printf("code    =[];\n");
	}

	printf("noise=[];\n");
	printf("codemean=[];\n");
	printf("codestd=[];\n");
	printf("abinvalid=[];\n");
	printf("cdinvalid=[];\n");
	printf("scale=[];\n");

	if (dl_size(noises) == 0)
		dl_append(noises, noise);

	for (k=0; k<dl_size(noises); k++) {

		noise = dl_get(noises, k);
		noisedist = noise / pixscale;

		codedelta = dl_new(256);
		codedists = dl_new(256);

		abInvalid = cdInvalid = 0;

		for (j=0; j<N; j++) {
			double midAB[2];
			double realcode[4];
			double code[4];

			midAB[0] = (realA[0] + realB[0]) / 2.0;
			midAB[1] = (realA[1] + realB[1]) / 2.0;

			// C
			// place C uniformly in the circle around the midpoint of AB.
			sample_field_in_circle(midAB, ABangle * 60.0 / pixscale, realC);

			// D
			// place D uniformly in the circle around the midpoint of AB.
			sample_field_in_circle(midAB, ABangle * 60.0 / pixscale, realD);

			{
				compute_field_code(realA, realB, realC, realD, realcode, NULL);
				realcodecx = realcode[0];
				realcodecy = realcode[1];
				realcodedx = realcode[2];
				realcodedy = realcode[3];
			}

			// permute A,B,C,D
			add_field_noise(realA, noisedist, A);
			add_field_noise(realB, noisedist, B);
			add_field_noise(realC, noisedist, C);
			add_field_noise(realD, noisedist, D);

			{
				double scale;
				compute_field_code(A, B, C, D, code, &scale);
				codecx = code[0];
				codecy = code[1];
				codedx = code[2];
				codedy = code[3];

				if ((scale < square(lowerAngle * 60.0 / pixscale)) ||
					(scale > square(upperAngle * 60.0 / pixscale)))
					abInvalid++;
				else if ((((codecx*codecx - codecx) + (codecy*codecy - codecy)) > 0.0) ||
						 (((codedx*codedx - codedx) + (codedy*codedy - codedy)) > 0.0))
					cdInvalid++;

				if (matlab)
					printf("scale(%i)=%g;\n", j+1, sqrt(scale)*pixscale/60.0);
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

			dl_append(codedists, sqrt(distsq(realcode, code, 4)));
		}

		{
			double mean, std;
			mean = 0.0;
			for (j=0; j<dl_size(codedists); j++)
				mean += dl_get(codedists, j);
			mean /= (double)dl_size(codedists);
			std = 0.0;
			for (j=0; j<dl_size(codedists); j++)
				std += square(dl_get(codedists, j) - mean);
			std /= ((double)dl_size(codedists) - 1);
			std = sqrt(std);

			if (print_codedists) {
				printf("codedists=[");
				for (j=0; j<dl_size(codedists); j++)
					printf("%g,", dl_get(codedists, j));
				printf("];\n");
			}

			printf("noise(%i)=%g; %%arcsec\n", k+1, noise);
			printf("codedistmean(%i)=%g;\n", k+1, mean);
			printf("codediststd(%i)=%g;\n", k+1, std);

			/*
			  mean = 0.0;
			  for (j=0; j<dl_size(codedelta); j++)
			  mean += dl_get(codedelta, j);
			  mean /= (double)dl_size(codedelta);
			  std = 0.0;
			  for (j=0; j<dl_size(codedelta); j++)
			  std += square(dl_get(codedelta, j) - mean);
			  std /= ((double)dl_size(codedelta) - 1);
			  printf("codemean(%i)=%g;\n", k+1, mean);
			  printf("codestd(%i)=%g;\n", k+1, std);
			*/
		}

		printf("abinvalid(%i) = %g;\n", k+1, abInvalid / (double)N);
		printf("cdinvalid(%i) = %g;\n", k+1, cdInvalid / (double)N);

		dl_free(codedelta);
		dl_free(codedists);
	}

	dl_free(noises);
	
	return 0;
}
