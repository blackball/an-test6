#include <math.h>
#include "hoggTweak.h"
double hoggExMaxStep(double *xx, double *varin, int nn, double varout,
		     double *logpin)
{
  int ii;
  double logpout, thisloglike, loglike, two[2], numerator;
  logpout= log(1.0-exp(*logpin));
  loglike= 0.0;
  for (ii= 0; ii < nn; ii++){
    two[0]= *logpin+hoggLogGaussian(xx[ii],varin[ii]);
    two[1]= logpout+hoggLogGaussian(xx[ii],varout);
    thisloglike= hoggLogsum(two,2);
    loglike+= thisloglike;
    two[0]-= thisloglike;
    if (ii == 0){
      numerator= two[0];
    } else {
      two[1]= numerator;
      numerator= hoggLogsum(two,2);
    }
  }
  *logpin= numerator-log((double) nn);
  return loglike;
}
