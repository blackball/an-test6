#ifndef MATHUTIL_H
#define MATHUTIL_H

#include "starutil.h"
#include "xylist.h"

double inverse_3by3(double *matrix);

void image_to_xyz(double uu, double vv, double* s, double* transform);

double *fit_transform(xy *ABCDpix, char order,
					  double* A, double* B, double* C, double* D);

double uniform_sample(double low, double high);

double gaussian_sample(double mean, double stddev);

inline int imax(int a, int b);

inline int imin(int a, int b);

inline double distsq_exceeds(double* d1, double* d2, int D, double limit);

inline double square(double d);

// note, this is cyclic
inline int inrange(double ra, double ralow, double rahigh);

inline double distsq(double* d1, double* d2, int D);

#endif
