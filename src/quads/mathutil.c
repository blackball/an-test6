#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mathutil.h"

inline int imax(int a, int b) {
	return (a > b) ? a : b;
}

inline int imin(int a, int b) {
	return (a < b) ? a : b;
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
	assert(s);
	assert(transform);

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

/*
  star = trans * (field; 1)

  star is 3 x N
  field is 2 x N and is supplemented by a row of ones.
  trans is 3 x 3

  "star" and "field" are stored in lexicographical order, so the element at
  (r, c) is stored at array[r + c*H], where H is the "height" of the
  matrix: 3 for "star", 2 for "field".

  "trans" is stored in the transposed order (to be compatible with
  image_to_xyz and the previous version of this function).

  S = T F
  S F' = T F F'
  S F' (F F')^-1 = T
*/
void fit_transform(double* star, double* field, int N, double* trans) {
	int r, c, k;
	double FFt[9];
	double det;
	double* R;
	double* F;

	// build F = (field; ones)
	F = malloc(3 * N * sizeof(double));
	for (c=0; c<N; r++) {
		// row 0
		F[0 + c*3] = field[0 + c*2];
		// row 1
		F[1 + c*3] = field[1 + c*2];
		// row 2
		F[2 + c*3] = 1.0;
	}

	// first compute  FFt  =   F   *   F'
	//               (3x3) = (3xN) * (Nx3)
	for (r=0; r<3; r++)
		for (c=0; c<3; c++) {
			// FFt(r,c) = sum_k F(r,k) * F'(k,c)
			//          = sum_k F(r,k) * F(c,k)
			// FFt[r+c*3] = sum_k F[r + k*3] * F[c + k*3].
			double acc = 0.0;
			for (k=0; k<N; k++)
				acc += F[r + k*3] * F[c + k*3];
			FFt[r + c*3] = acc;
		}

	// invert FFt in-place.
	det = inverse_3by3(FFt);

	if (det < 0)
		fprintf(stderr, "WARNING (fit_transform) -- determinant<0\n");

	if (det == 0.0) {
		fprintf(stderr, "ERROR (fit_transform) -- determinant zero\n");
		return;
	}

	R = malloc(N * 3 * sizeof(double));

	// compute   R   =   F'  *  FFt  = F' inv(F F')
	//         (Nx3) = (Nx3) * (3x3)
	for (r=0; r<N; r++)
		for (c=0; c<3; c++) {
			// R(r,c) = sum_k F'(r,k) * FFt(k,c)
			//        = sum_k F(k,r) * FFt(k,c)
			//        = sum_k F[k + r*3] * FFt[k + c*3]
			double acc = 0.0;
			for (k=0; k<3; k++)
				acc += F[k + r*3] * FFt[k + c*3];
			R[r + c*N] = acc;
		}

	// finally, compute   T   =   S   *   R   = S F' inv(F F')
	//                  (3x3) = (3xN) * (Nx3)
	for (r=0; r<3; r++)
		for (c=0; c<3; c++) {
			// T(r,c) = sum_k S(r,k) * R(k,c)
			//        = sum_k S[r + k*3] * R[k + c*N]
			double acc = 0.0;
			for (k=0; k<N; k++)
				acc += star[r + k*3] * R[k + c*N];
			// "trans" is stored in transposed order.
			trans[c + r*3] = acc;
		}

	free(F);
	free(R);
}

