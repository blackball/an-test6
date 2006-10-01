#ifndef EZFITS_H
#define EZFITS_H


#include "fitsio.h"


int ezwriteimage(char* fn, int datatype, void* data, int w, int h);

int ezscatter(char* fn, double* x, double *y,
                   double* a, double *d, int n);

#endif
