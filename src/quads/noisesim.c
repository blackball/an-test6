#include <math.h>
#include <stdio.h>

#include "starutil.h"
#include "mathutil.h"

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
	double ABangle = 4.5; // arcminutes
	double noise = 3; // arcseconds

	double noisedist;
	double noiseangle;
	double noiseval;

	double realA[3];
	double realB[3];
	double realC[3];
	double realcodex, realcodey;

	double A[3];
	double B[3];
	double C[3];
	double codex, codey;

	double va[3], vb[3];

	double ra, dec;
	int i;
	int j;
	int N=1000;

	noisedist = sqrt(arcsec2distsq(noise));

	// A
	ra = 0.0;
	dec = 0.0;
	radec2xyzarr(ra, dec, realA);

	// B
	ra = deg2rad(ABangle / 60.0);
	dec = 0.0;
	radec2xyzarr(ra, dec, realB);

	printf("realcode=[];\n");
	printf("code    =[];\n");

	for (j=0; j<N; j++) {

		// C
		// place C uniformly in the circle around the midpoint of AB.
		star_midpoint(realC, realA, realB);
		tan_vectors(realC, va, vb);
		noiseval = sqrt(arcsec2distsq(60.0 * ABangle) * 0.5 * uniform_sample(0.0, 1.0));
		noiseangle = uniform_sample(0.0, 2.0*M_PI);
		for (i=0; i<3; i++)
			realC[i] +=
				cos(noiseangle) * noiseval * va[i] +
				sin(noiseangle) * noiseval * vb[i];

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
			realcodex =  ADx * costheta + ADy * sintheta;
			realcodey = -ADx * sintheta + ADy * costheta;
		}

		// permute A
		tan_vectors(realA, va, vb);
		noiseval = gaussian_sample(0.0, noisedist);
		noiseangle = uniform_sample(0.0, 2.0*M_PI);
		for (i=0; i<3; i++)
			A[i] = realA[i] +
				cos(noiseangle) * noiseval * va[i] +
				sin(noiseangle) * noiseval * vb[i];

		// permute B
		tan_vectors(realB, va, vb);
		noiseval = gaussian_sample(0.0, noisedist);
		noiseangle = uniform_sample(0.0, 2.0*M_PI);
		for (i=0; i<3; i++)
			B[i] = realB[i] +
				cos(noiseangle) * noiseval * va[i] +
				sin(noiseangle) * noiseval * vb[i];

		// permute C
		tan_vectors(realC, va, vb);
		noiseval = gaussian_sample(0.0, noisedist);
		noiseangle = uniform_sample(0.0, 2.0*M_PI);
		for (i=0; i<3; i++)
			C[i] = realC[i] +
				cos(noiseangle) * noiseval * va[i] +
				sin(noiseangle) * noiseval * vb[i];

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
			codex =  ADx * costheta + ADy * sintheta;
			codey = -ADx * sintheta + ADy * costheta;
		}

		printf("realcode(%i,:)=[%g,%g];\n", j+1, realcodex, realcodey);
		printf("code(%i,:)    =[%g,%g];\n", j+1, codex, codey);

	}


	return 0;
}
