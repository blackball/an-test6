#include <math.h>

#include "noise.h"
#include "mathutil.h"
#include "starutil.h"

void sample_field_in_circle(double* center, double radius,
							double* point) {
	double noisemag, noiseangle;
	noisemag = radius * sqrt(0.25 * uniform_sample(0.0, 1.0));
	noiseangle = uniform_sample(0.0, 2.0*M_PI);
	point[0] = center[0] + cos(noiseangle) * noisemag;
	point[1] = center[1] + sin(noiseangle) * noisemag;
}

void sample_star_in_circle(double* center, double ABangle,
						   double* point) {
	double va[3], vb[3];
	double noisemag, noiseangle;
	int i;

	tan_vectors(center, va, vb);
	noisemag = sqrt(arcsec2distsq(60.0 * ABangle) * 0.25 * uniform_sample(0.0, 1.0));
	noiseangle = uniform_sample(0.0, 2.0*M_PI);
	for (i=0; i<3; i++)
		point[i] = center[i] +
			cos(noiseangle) * noisemag * va[i] +
			sin(noiseangle) * noisemag * vb[i];
	normalize_3(point);
}

void add_star_noise(double* real, double noisevar, double* noisy) {
	double va[3], vb[3];
	double noisemag, noiseangle;
	int i;

	tan_vectors(real, va, vb);
	// magnitude of noise
	noisemag = gaussian_sample(0.0, noisevar);
	// direction of noise
	noiseangle = uniform_sample(0.0, 2.0*M_PI);
	for (i=0; i<3; i++)
		noisy[i] = real[i] +
			cos(noiseangle) * noisemag * va[i] +
			sin(noiseangle) * noisemag * vb[i];
	normalize_3(real);
}

void add_field_noise(double* real, double noisevar, double* noisy) {
	double noisemag, noiseangle;
	// magnitude of noise
	noisemag = gaussian_sample(0.0, noisevar);
	// direction of noise
	noiseangle = uniform_sample(0.0, 2.0*M_PI);
	noisy[0] = real[0] + cos(noiseangle) * noisemag;
	noisy[1] = real[1] + sin(noiseangle) * noisemag;
}

void compute_star_code(double* A, double* B, double* C, double* D,
					   double* code) {
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
	code[0] =  ADx * costheta + ADy * sintheta;
	code[1] = -ADx * sintheta + ADy * costheta;
	star_coords(D, midAB, &Dx, &Dy);
	ADx = Dx - Ax;
	ADy = Dy - Ay;
	code[2] =  ADx * costheta + ADy * sintheta;
	code[3] = -ADx * sintheta + ADy * costheta;
}

void compute_field_code(double* A, double* B, double* C, double* D,
						double* code, double* p_scale) {
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

	Cx = C[0];
	Cy = C[1];
	Cx -= Ax;
	Cy -= Ay;
	xxtmp = Cx;
	Cx =     Cx * costheta + Cy * sintheta;
	Cy = -xxtmp * sintheta + Cy * costheta;
	code[0] = Cx;
	code[1] = Cy;

	Cx = D[0];
	Cy = D[1];
	Cx -= Ax;
	Cy -= Ay;
	xxtmp = Cx;
	Cx =     Cx * costheta + Cy * sintheta;
	Cy = -xxtmp * sintheta + Cy * costheta;
	code[2] = Cx;
	code[3] = Cy;

	if (p_scale)
		*p_scale = scale;
}


