#include <stdio.h>
#include <math.h>
#include "hoggTweak.h"
double hoggExMaxStep(double *xx, double *varin, int nn, double varout,
		     double *logpin)
{
  int ii;
  double logpout, thisloglike, loglike, two[2], numerator;
  printf("hoggExMaxStep: at start logpin = %lf\n",*logpin);
  logpout= log(1.0-exp(*logpin));
  loglike= 0.0;
  numerator= 0.0;
  for (ii= 0; ii < nn; ii++){
    two[0]= *logpin+hoggLogGaussian(xx[ii],varin[ii]);
    two[1]= logpout+hoggLogGaussian(xx[ii],varout);
    thisloglike= hoggLogsum(two,2);
    loglike+= thisloglike;
    two[0]-= thisloglike;
    two[1]= numerator;
    numerator= hoggLogsum(two,2);
  }
  printf("hoggExMaxStep: log likelihood = %lf\n",loglike);
  *logpin= numerator-log((double) nn);
  printf("hoggExMaxStep: at finish logpin = %lf\n",*logpin);
  return loglike;
}
