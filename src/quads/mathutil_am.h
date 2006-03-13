#ifndef MATHUTIL_AM_H
#define MATHUTIL_AM_H

#include "starutil_am.h"

//unsigned long int choose(unsigned int nn, unsigned int mm);

double inverse_3by3(double *matrix);

void image_to_xyz_old(double uu, double vv, star *s, double *transform);

double *fit_transform_old(xy *ABCDpix, char order, star *A, star *B, star *C, star *D);

#endif
