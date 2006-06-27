#ifndef MATHUTIL_H
#define MATHUTIL_H

#include "starutil.h"
#include "xylist.h"
#include "keywords.h"

double inverse_3by3(double *matrix);

void image_to_xyz(double uu, double vv, double* s, double* transform);

void fit_transform(double* star, double* field, int N, double* trans);

double uniform_sample(double low, double high);

double gaussian_sample(double mean, double stddev);

Const Inline int imax(int a, int b);

Const Inline int imin(int a, int b);

Inline double distsq_exceeds(double* d1, double* d2, int D, double limit);

Const Inline double square(double d);

// note, this is cyclic
Const Inline int inrange(double ra, double ralow, double rahigh);

Inline double distsq(double* d1, double* d2, int D);

#endif
