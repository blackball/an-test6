#ifndef MATHUTIL_H
#define MATHUTIL_H

#include "starutil.h"
#include "xylist.h"

// do we use this?
//unsigned long int choose(unsigned int nn, unsigned int mm);

double inverse_3by3(double *matrix);

void image_to_xyz(double uu, double vv, double* s, double* transform);

double *fit_transform(xy *ABCDpix, char order,
					  double* A, double* B, double* C, double* D);

double uniform_sample(double low, double high);

#endif
