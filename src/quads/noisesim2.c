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

const char* OPTIONS = "e:n:ma:u:l:";

int main(int argc, char** args) {
	int argchar;

	double ABangle = 4.5; // arcminutes
	double pixscale = 0.396; // arcsec / pixel
	double noise = 3; // arcsec

	double lowerAngle = 4.0;
	double upperAngle = 5.0;

	double noisedist;
	double noiseangle;
	double noiseval;

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

	dl* codedelta;
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

	if (dl_size(noises) == 0)
		dl_append(noises, noise);

	for (k=0; k<dl_size(noises); k++) {

		noise = dl_get(noises, k);
		noisedist = noise / pixscale;

		codedelta = dl_new(256);

		abInvalid = cdInvalid = 0;

		for (j=0; j<N; j++) {

			// C
			// place C uniformly in the circle around the midpoint of AB.
			realC[0] = (realA[0] + realB[0]) / 2.0;
			realC[1] = (realA[1] + realB[1]) / 2.0;
			noiseval = ABangle * 60.0 / pixscale * sqrt(0.25 * uniform_sample(0.0, 1.0));
			noiseangle = uniform_sample(0.0, 2.0*M_PI);
			realC[0] += cos(noiseangle) * noiseval;
			realC[1] += sin(noiseangle) * noiseval;

			// D
			// place D uniformly in the circle around the midpoint of AB.
			realD[0] = (realA[0] + realB[0]) / 2.0;
			realD[1] = (realA[1] + realB[1]) / 2.0;
			noiseval = ABangle * 60.0 / pixscale * sqrt(0.25 * uniform_sample(0.0, 1.0));
			noiseangle = uniform_sample(0.0, 2.0*M_PI);
			realD[0] += cos(noiseangle) * noiseval;
			realD[1] += sin(noiseangle) * noiseval;

			{
				double Ax, Ay, Bx, By, dx, dy, scale;
				double costheta, sintheta;
				double Cx, Cy, xxtmp;

				Ax = realA[0];
				Ay = realA[1];
				Bx = realB[0];
				By = realB[1];
				dx = Bx - Ax;
				dy = By - Ay;
				scale = dx*dx + dy*dy;
				costheta = (dy + dx) / scale;
				sintheta = (dy - dx) / scale;

				Cx = realC[0];
				Cy = realC[1];
				Cx -= Ax;
				Cy -= Ay;
				xxtmp = Cx;
				Cx =     Cx * costheta + Cy * sintheta;
				Cy = -xxtmp * sintheta + Cy * costheta;
				realcodecx = Cx;
				realcodecy = Cy;

				Cx = realD[0];
				Cy = realD[1];
				Cx -= Ax;
				Cy -= Ay;
				xxtmp = Cx;
				Cx =     Cx * costheta + Cy * sintheta;
				Cy = -xxtmp * sintheta + Cy * costheta;
				realcodedx = Cx;
				realcodedy = Cy;
			}

			// permute A
			// magnitude of noise
			noiseval = gaussian_sample(0.0, noisedist);
			// direction of noise
			noiseangle = uniform_sample(0.0, 2.0*M_PI);
			A[0] = realA[0] + cos(noiseangle) * noiseval;
			A[1] = realA[1] + sin(noiseangle) * noiseval;

			// permute B
			noiseval = gaussian_sample(0.0, noisedist);
			noiseangle = uniform_sample(0.0, 2.0*M_PI);
			B[0] = realB[0] + cos(noiseangle) * noiseval;
			B[1] = realB[1] + sin(noiseangle) * noiseval;

			// permute C
			noiseval = gaussian_sample(0.0, noisedist);
			noiseangle = uniform_sample(0.0, 2.0*M_PI);
			C[0] = realC[0] + cos(noiseangle) * noiseval;
			C[1] = realC[1] + sin(noiseangle) * noiseval;

			// permute D
			noiseval = gaussian_sample(0.0, noisedist);
			noiseangle = uniform_sample(0.0, 2.0*M_PI);
			D[0] = realD[0] + cos(noiseangle) * noiseval;
			D[1] = realD[1] + sin(noiseangle) * noiseval;

			{
				double Ax, Ay, Bx, By, dx, dy, scale;
				double costheta, sintheta;
				double Cx, Cy, xxtmp;

				Ax = A[0];
				Ay = A[1];
				Bx = B[0];
				By = B[1];
				dx = Bx - Ax;
				dy = By - Ay;
				scale = dx*dx + dy*dy;
				costheta = (dy + dx) / scale;
				sintheta = (dy - dx) / scale;

				if ((scale < square(lowerAngle * 60.0 / pixscale)) ||
					(scale > square(upperAngle * 60.0 / pixscale))) {
					abInvalid++;
				}

				Cx = C[0];
				Cy = C[1];
				Cx -= Ax;
				Cy -= Ay;
				xxtmp = Cx;
				Cx =     Cx * costheta + Cy * sintheta;
				Cy = -xxtmp * sintheta + Cy * costheta;
				codecx = Cx;
				codecy = Cy;

				Cx = D[0];
				Cy = D[1];
				Cx -= Ax;
				Cy -= Ay;
				xxtmp = Cx;
				Cx =     Cx * costheta + Cy * sintheta;
				Cy = -xxtmp * sintheta + Cy * costheta;
				codedx = Cx;
				codedy = Cy;

				//if (((Cx*Cx - Cx) + (Cy*Cy - Cy)) > 0.0)
				//cdInvalid++;

				if ((((codecx*codecx - codecx) + (codecy*codecy - codecy)) > 0.0) ||
					(((codedx*codedx - codedx) + (codedy*codedy - codedy)) > 0.0))
					cdInvalid++;
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

		printf("abinvalid(%i) = %g;\n", k+1, abInvalid / (double)N);
		printf("cdinvalid(%i) = %g;\n", k+1, cdInvalid / (double)N);

		dl_free(codedelta);
	}

	dl_free(noises);
	
	return 0;
}
