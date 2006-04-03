#include <math.h>
#include <stdio.h>
#include <string.h>
#include "mathutil.h"

inline int imax(int a, int b) {
	return (a > b) ? a : b;
}

inline double distsq_exceeds(double* d1, double* d2, int D, double limit) {
    double dist2;
    int i;
    dist2 = 0.0;
    for (i=0; i<D; i++) {
		dist2 += square(d1[i] - d2[i]);
		if (dist2 > limit)
			return 1;
    }
	return 0;
}

inline double distsq(double* d1, double* d2, int D) {
    double dist2;
    int i;
    dist2 = 0.0;
    for (i=0; i<D; i++) {
		dist2 += square(d1[i] - d2[i]);
    }
    return dist2;
}

inline double square(double d) {
    return d*d;
}

inline int inrange(double ra, double ralow, double rahigh) {
    if (ralow < rahigh) {
		if (ra >= ralow && ra <= rahigh)
            return 1;
        return 0;
    }

    /* handle wraparound properly */
    //if (ra <= ralow && ra >= rahigh)
    if (ra >= ralow || ra <= rahigh)
        return 1;
    return 0;
}

#define GAUSSIAN_SAMPLE_INVALID -1e300

double gaussian_sample(double mean, double stddev) {
	// from http://www.taygeta.com/random/gaussian.html
	// Algorithm by Dr. Everett (Skip) Carter, Jr.
	static double y2 = GAUSSIAN_SAMPLE_INVALID;
	double x1, x2, w, y1;

	// this algorithm generates random samples in pairs; the INVALID
	// jibba-jabba stores the second value until the next time the
	// function is called.

	if (y2 != GAUSSIAN_SAMPLE_INVALID) {
		y1 = y2;
		y2 = GAUSSIAN_SAMPLE_INVALID;
		return mean + y1 * stddev;
	}
	do {
		x1 = uniform_sample(-0.5, 0.5);
		x2 = uniform_sample(-0.5, 0.5);
		w = x1 * x1 + x2 * x2;
	} while ( w >= 1.0 );

	w = sqrt( (-2.0 * log(w)) / w );
	y1 = x1 * w;
	y2 = x2 * w;
	return mean + y1 * stddev;
}

double uniform_sample(double low, double high) {
	return low + (high - low)*((double)rand() / (double)RAND_MAX);
}

/* computes IN PLACE the inverse of a 3x3 matrix stored as a 9-vector
   with the first ROW of the matrix in positions 0-2, the second ROW
   in positions 3-5, and the last ROW in positions 6-8. */
double inverse_3by3(double *matrix)
{
	double det;
	double a11, a12, a13, a21, a22, a23, a31, a32, a33;
	double b11, b12, b13, b21, b22, b23, b31, b32, b33;

	a11 = matrix[0];
	a12 = matrix[1];
	a13 = matrix[2];
	a21 = matrix[3];
	a22 = matrix[4];
	a23 = matrix[5];
	a31 = matrix[6];
	a32 = matrix[7];
	a33 = matrix[8];

	det = a11 * ( a22 * a33 - a23 * a32 ) +
	      a12 * ( a23 * a31 - a21 * a33 ) +
	      a13 * ( a21 * a32 - a22 * a31 );

	if (det == 0.0) {
		return det;
	}

	b11 = + ( a22 * a33 - a23 * a32 ) / det;
	b12 = - ( a12 * a33 - a13 * a32 ) / det;
	b13 = + ( a12 * a23 - a13 * a22 ) / det;

	b21 = - ( a21 * a33 - a23 * a31 ) / det;
	b22 = + ( a11 * a33 - a13 * a31 ) / det;
	b23 = - ( a11 * a23 - a13 * a21 ) / det;

	b31 = + ( a21 * a32 - a22 * a31 ) / det;
	b32 = - ( a11 * a32 - a12 * a31 ) / det;
	b33 = + ( a11 * a22 - a12 * a21 ) / det;

	matrix[0] = b11;
	matrix[1] = b12;
	matrix[2] = b13;
	matrix[3] = b21;
	matrix[4] = b22;
	matrix[5] = b23;
	matrix[6] = b31;
	matrix[7] = b32;
	matrix[8] = b33;

	//fprintf(stderr,"matrix determinant = %g\n",det);

	return (det);
}


void image_to_xyz(double uu, double vv, double* s, double* transform) {
	double x, y, z;
	double length;
	if (s == NULL || transform == NULL)
		return;

	x = uu * transform[0] +
		vv * transform[1] +
		transform[2];

	y = uu * transform[3] +
		vv * transform[4] +
		transform[5];

	z = uu * transform[6] +
		vv * transform[7] +
		transform[8];

	length = sqrt(x*x + y*y + z*z);

	x /= length;
	y /= length;
	z /= length;

	s[0] = x;
	s[1] = y;
	s[2] = z;
}

double *fit_transform(xy *ABCDpix, char order,
					  double* A, double* B, double* C, double* D)
{
	double det, uu, uv, vv, sumu, sumv;
	char oA = 0, oB = 1, oC = 2, oD = 3;
	double Au, Av, Bu, Bv, Cu, Cv, Du, Dv;
	double matQ[9];
	double matR[12];

	if (order == ABCD_ORDER) {
		oA = 0;
		oB = 1;
		oC = 2;
		oD = 3;
	}
	if (order == BACD_ORDER) {
		oA = 1;
		oB = 0;
		oC = 2;
		oD = 3;
	}
	if (order == ABDC_ORDER) {
		oA = 0;
		oB = 1;
		oC = 3;
		oD = 2;
	}
	if (order == BADC_ORDER) {
		oA = 1;
		oB = 0;
		oC = 3;
		oD = 2;
	}

	// image plane coordinates of A,B,C,D
	Au = xy_refx(ABCDpix, oA);
	Av = xy_refy(ABCDpix, oA);
	Bu = xy_refx(ABCDpix, oB);
	Bv = xy_refy(ABCDpix, oB);
	Cu = xy_refx(ABCDpix, oC);
	Cv = xy_refy(ABCDpix, oC);
	Du = xy_refx(ABCDpix, oD);
	Dv = xy_refy(ABCDpix, oD);

	//fprintf(stderr,"Image ABCD = (%lf,%lf) (%lf,%lf) (%lf,%lf) (%lf,%lf)\n",
	//  	    Au,Av,Bu,Bv,Cu,Cv,Du,Dv);

	// define M to be the 3x4 matrix [Au,Bu,Cu,Du;ones(1,4)]
	// define X to be the 3x4 matrix [Ax,Bx,Cx,Dx;Ay,By,Cy,Dy;Az,Bz,Cz,Dz]

	// set Q to be the 3x3 matrix  M*M'
	uu = Au * Au + Bu * Bu + Cu * Cu + Du * Du;
	uv = Au * Av + Bu * Bv + Cu * Cv + Du * Dv;
	vv = Av * Av + Bv * Bv + Cv * Cv + Dv * Dv;
	sumu = Au + Bu + Cu + Du;
	sumv = Av + Bv + Cv + Dv;
	matQ[0] = uu;
	matQ[1] = uv;
	matQ[2] = sumu;
	matQ[3] = uv;
	matQ[4] = vv;
	matQ[5] = sumv;
	matQ[6] = sumu;
	matQ[7] = sumv;
	matQ[8] = 4.0;

	// take the inverse of Q in-place, so Q=inv(M*M')
	det = inverse_3by3(matQ);

	//fprintf(stderr,"det=%.12g\n",det);
	if (det < 0)
		fprintf(stderr, "WARNING (fit_transform) -- determinant<0\n");

	if (det == 0.0) {
		fprintf(stderr, "ERROR (fit_transform) -- determinant zero\n");
		return (NULL);
	}

	//fprintf(stderr, "det=%g\n", det);

	// set R to be the 4x3 matrix M'*inv(M*M')=M'*Q
	matR[0]  = matQ[0] * Au + matQ[3] * Av + matQ[6];
	matR[1]  = matQ[1] * Au + matQ[4] * Av + matQ[7];
	matR[2]  = matQ[2] * Au + matQ[5] * Av + matQ[8];
	matR[3]  = matQ[0] * Bu + matQ[3] * Bv + matQ[6];
	matR[4]  = matQ[1] * Bu + matQ[4] * Bv + matQ[7];
	matR[5]  = matQ[2] * Bu + matQ[5] * Bv + matQ[8];
	matR[6]  = matQ[0] * Cu + matQ[3] * Cv + matQ[6];
	matR[7]  = matQ[1] * Cu + matQ[4] * Cv + matQ[7];
	matR[8]  = matQ[2] * Cu + matQ[5] * Cv + matQ[8];
	matR[9]  = matQ[0] * Du + matQ[3] * Dv + matQ[6];
	matR[10] = matQ[1] * Du + matQ[4] * Dv + matQ[7];
	matR[11] = matQ[2] * Du + matQ[5] * Dv + matQ[8];

	// set Q to be the 3x3 matrix X*R

	matQ[0] = A[0] * matR[0] + B[0] * matR[3] +
	          C[0] * matR[6] + D[0] * matR[9];
	matQ[1] = A[0] * matR[1] + B[0] * matR[4] +
	          C[0] * matR[7] + D[0] * matR[10];
	matQ[2] = A[0] * matR[2] + B[0] * matR[5] +
	          C[0] * matR[8] + D[0] * matR[11];

	matQ[3] = A[1] * matR[0] + B[1] * matR[3] +
	          C[1] * matR[6] + D[1] * matR[9];
	matQ[4] = A[1] * matR[1] + B[1] * matR[4] +
	          C[1] * matR[7] + D[1] * matR[10];
	matQ[5] = A[1] * matR[2] + B[1] * matR[5] +
	          C[1] * matR[8] + D[1] * matR[11];

	matQ[6] = A[2] * matR[0] + B[2] * matR[3] +
	          C[2] * matR[6] + D[2] * matR[9];
	matQ[7] = A[2] * matR[1] + B[2] * matR[4] +
	          C[2] * matR[7] + D[2] * matR[10];
	matQ[8] = A[2] * matR[2] + B[2] * matR[5] +
		      C[2] * matR[8] + D[2] * matR[11];

	{
		double* copyQ = (double*)malloc(9 * sizeof(double));
		memcpy(copyQ, matQ, 9 * sizeof(double));
		return copyQ;
	}
}



