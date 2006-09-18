#ifndef __MATHEXT_HPP__
#define __MATHEXT_HPP__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>
#include <math.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// The tiniest of values (double).
#define TINY              1.0e-50

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * General math calculatations, number generators, not available via <math.h>.
 */

// Return log-base-2 of 'val'.
double log2(double val);
// Return log-base-'base' of 'val'.
double logb(double val, double base);
// Round 'val' to nearest integer value.
double round(double val);
// Round 'val' to nearest integer value.
float  round(float val);
// Return modulus of 'v1' to base 'base'.
double mod(double v1, double base);
// Return modulus of 'v1' to base 'base'.
float  mod(float v1, float base);
// Return a uniform random double.
double randD();
// Return a uniform random float.
float  randF();
// Return a vector of uniform random doubles, each multiplied by 'mult' and then
// having 'add' added to them.
void   randVD(double* dest, DWORD cnt, double mult = (double)1.0, double add = 0);
// Return a vector of uniform random float, each multiplied by 'mult' and then
// having 'add' added to them.
void   randVF(float* dest, DWORD cnt, float mult = (float)1.0, float add = 0);
// Returns a guassian random double with standard deviation 'stddev' and mean 'mean'.
double randDN(double stddev = 1.0, double mean = 0);
// Returns a vector of 'cnt' guassian random doubles with standard deviation 'stddev' and mean 'mean'.
void   randVDN(double* dest, DWORD cnt, double stddev = 1.0, double mean = 0);
// Returns two double angles 0 <= theta <= 2PI, 0 <= phi <= PI, denoting a uniform random point
// on the unit sphere.
void   randSphereD(double *theta, double *phi);
// Returns a double 3-vector denoting a uniform random point on the unit sphere.
void   randSphereD(double *dest);
// Returns two float angles 0 <= theta <= 2PI, 0 <= phi <= PI, denoting a uniform random point
// on the unit sphere.
void   randSphereF(float *theta, float *phi);
// Returns a float 3-vector denoting a uniform random point on the unit sphere.
void   randSphereF(float *dest);

/* -------------------------------------------------- */

/*
 * Slightly more specific combinatorial and probability calculatations.
 */

// Returns the factorial of 'x'.
double factorial(DWORD x);
// Returns 'n' choose 'k'.
double choose(DWORD n, DWORD k);
// Returns the probability of 'x' given the poisson distribution with parameter 'lambda' >= 0.
double poisspdf(DWORD x, double lambda);
// Returns the sum of the probability of [0,max] given the poisson distribution with parameter 'lambda'.
double sumPoiss(DWORD max, double lambda);
// Returns the sum of the probability of [min,inf] given the poisson distribution with parameter 'lambda'.
// The sum is accurate up to the first probability found below 'acc', and all probabilities are returned
// via 'val', the number of elements in 'vals' is returned via maxval.
double sumUpPoiss(DWORD *maxval, double **vals, DWORD min, double lambda, double acc);
// Returns the probability of 'x' given the geometric distribution with parameter 'p' in [0,1]
double geopdf(DWORD x, double p);

/* -------------------------------------------------- */

/*
 * Linear system solving functions.
 */

// Creates LU decomposition of matrix a, which is expected to be
// an 'n'-by-'n' matrix. 'indx' is an 'n'-vector which will be filled
// with row permutation information. 'd' is a double that will contain 
// +/- 1 indicating row permutation count as even/odd.
BOOL32 luDCmp(double *a, int n, int *indx, double *d);
// Finishes solving a x == b. 'a' is the matrix 'a' resulting from luDCmp. 
// 'indx' is the 'indx' vector returned from luDCmp, 'b' is the vector in 
// a x == b, and is filled with the result 'x'
void   luBackSub(double *a, int n, int *indx, double *b);

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif
