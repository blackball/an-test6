#ifndef mathutil_H
#define mathutil_H
#include "starutil.h"

unsigned long int choose(unsigned int nn,unsigned int mm);
double inverse_3by3(double *matrix);
void image_to_xyz(double uu, double vv, star *s, double *transform);
double *fit_transform(xy *ABCDpix,char order,star *A,star *B,star *C,star *D);

ivec *box_containing_most_points(dyv *x,dyv *y,double bx, double by);

#endif
