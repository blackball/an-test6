#include <math.h>
#include <stdio.h>

#include "starutil.h"
#include "mathutil.h"

const char* OPTIONS = "e:n:ma:";

void tan_vectors(double* pt, double* vec1, double* vec2) {
	double etax, etay, etaz, xix, xiy, xiz, eta_norm;
	double inv_en;
	// eta is a vector perpendicular to pt
	etax = -pt[1];
	etay =  pt[0];
	etaz = 0.0;
	eta_norm = hypot(etax, etay); //sqrt(etax * etax + etay * etay);
	inv_en = 1.0 / eta_norm;
	etax *= inv_en;
	etay *= inv_en;

	vec1[0] = etax;
	vec1[1] = etay;
	vec1[2] = etaz;

	// xi =  pt cross eta
	xix = -pt[2] * etay;
	xiy =  pt[2] * etax;
	xiz =  pt[0] * etay - pt[1] * etax;
	// xi has unit length since pt and eta have unit length.

	vec2[0] = xix;
	vec2[1] = xiy;
	vec2[2] = xiz;
}

int main(int argc, char** args) {
	int argchar;

	double ABangle = 4.5; // arcminutes
	double noise = 3; // arcseconds

	double noisedist;
	double noiseangle;
	double noiseval;

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

	double va[3], vb[3];

	double ra, dec;
	int i;
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

			// C
			// place C uniformly in the circle around the midpoint of AB.
			star_midpoint(realC, realA, realB);
			tan_vectors(realC, va, vb);
			noiseval = sqrt(arcsec2distsq(60.0 * ABangle) * 0.25 * uniform_sample(0.0, 1.0));
			noiseangle = uniform_sample(0.0, 2.0*M_PI);
			for (i=0; i<3; i++)
				realC[i] +=
					cos(noiseangle) * noiseval * va[i] +
					sin(noiseangle) * noiseval * vb[i];
			normalize_3(realC);

			// D
			// place D uniformly in the circle around the midpoint of AB.
			star_midpoint(realD, realA, realB);
			tan_vectors(realD, va, vb);
			noiseval = sqrt(arcsec2distsq(60.0 * ABangle) * 0.25 * uniform_sample(0.0, 1.0));
			noiseangle = uniform_sample(0.0, 2.0*M_PI);
			for (i=0; i<3; i++)
				realD[i] +=
					cos(noiseangle) * noiseval * va[i] +
					sin(noiseangle) * noiseval * vb[i];
			normalize_3(realD);

			/*{
			  double midAB[3];
			  double dA, dB, dC, dD;
			  star_midpoint(midAB, realA, realB);
			  dA = sqrt(distsq(midAB, realA, 3));
			  dB = sqrt(distsq(midAB, realB, 3));
			  dC = sqrt(distsq(midAB, realC, 3));
			  dD = sqrt(distsq(midAB, realD, 3));
			  printf("dA=%g;\n", dA);
			  printf("dB=%g;\n", dB);
			  printf("dC=%g;\n", dC);
			  printf("dD=%g;\n", dD);
			  }*/

			{
				double midAB[3];
				double Ax, Ay;
				double Bx, By;
				double scale, invscale;
				double ABx, ABy;
				double costheta, sintheta;
				double Dx, Dy;
				double ADx, ADy;

				star_midpoint(midAB, realA, realB);
				star_coords(realA, midAB, &Ax, &Ay);
				star_coords(realB, midAB, &Bx, &By);
				ABx = Bx - Ax;
				ABy = By - Ay;
				scale = (ABx * ABx) + (ABy * ABy);
				invscale = 1.0 / scale;
				costheta = (ABy + ABx) * invscale;
				sintheta = (ABy - ABx) * invscale;
				star_coords(realC, midAB, &Dx, &Dy);
				ADx = Dx - Ax;
				ADy = Dy - Ay;
				realcodecx =  ADx * costheta + ADy * sintheta;
				realcodecy = -ADx * sintheta + ADy * costheta;
				star_coords(realD, midAB, &Dx, &Dy);
				ADx = Dx - Ax;
				ADy = Dy - Ay;
				realcodedx =  ADx * costheta + ADy * sintheta;
				realcodedy = -ADx * sintheta + ADy * costheta;
			}

			// permute A
			tan_vectors(realA, va, vb);
			noiseval = gaussian_sample(0.0, noisedist);
			noiseangle = uniform_sample(0.0, 2.0*M_PI);
			for (i=0; i<3; i++)
				A[i] = realA[i] +
					cos(noiseangle) * noiseval * va[i] +
					sin(noiseangle) * noiseval * vb[i];
			normalize_3(realA);

			// permute B
			tan_vectors(realB, va, vb);
			noiseval = gaussian_sample(0.0, noisedist);
			noiseangle = uniform_sample(0.0, 2.0*M_PI);
			for (i=0; i<3; i++)
				B[i] = realB[i] +
					cos(noiseangle) * noiseval * va[i] +
					sin(noiseangle) * noiseval * vb[i];
			normalize_3(realB);

			// permute C
			tan_vectors(realC, va, vb);
			noiseval = gaussian_sample(0.0, noisedist);
			noiseangle = uniform_sample(0.0, 2.0*M_PI);
			for (i=0; i<3; i++)
				C[i] = realC[i] +
					cos(noiseangle) * noiseval * va[i] +
					sin(noiseangle) * noiseval * vb[i];
			normalize_3(realC);

			// permute D
			tan_vectors(realD, va, vb);
			noiseval = gaussian_sample(0.0, noisedist);
			noiseangle = uniform_sample(0.0, 2.0*M_PI);
			for (i=0; i<3; i++)
				D[i] = realD[i] +
					cos(noiseangle) * noiseval * va[i] +
					sin(noiseangle) * noiseval * vb[i];
			normalize_3(realD);

			{
				double midAB[3];
				double Ax, Ay;
				double Bx, By;
				double scale, invscale;
				double ABx, ABy;
				double costheta, sintheta;
				double Dx, Dy;
				double ADx, ADy;

				star_midpoint(midAB, A, B);
				star_coords(A, midAB, &Ax, &Ay);
				star_coords(B, midAB, &Bx, &By);
				ABx = Bx - Ax;
				ABy = By - Ay;
				scale = (ABx * ABx) + (ABy * ABy);
				invscale = 1.0 / scale;
				costheta = (ABy + ABx) * invscale;
				sintheta = (ABy - ABx) * invscale;
				star_coords(C, midAB, &Dx, &Dy);
				ADx = Dx - Ax;
				ADy = Dy - Ay;
				codecx =  ADx * costheta + ADy * sintheta;
				codecy = -ADx * sintheta + ADy * costheta;
				star_coords(D, midAB, &Dx, &Dy);
				ADx = Dx - Ax;
				ADy = Dy - Ay;
				codedx =  ADx * costheta + ADy * sintheta;
				codedy = -ADx * sintheta + ADy * costheta;
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
