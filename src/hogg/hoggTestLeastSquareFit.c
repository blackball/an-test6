#include <math.h>
#include <stdio.h>
#include "hoggTweak.h"
#include "hoggMath.h"
int hoggTestLeastSquareFit()
{
  int ii;
  double yy[3],xx[6],aa[1],diff;
  printf("hoggTestLeastSquareFit: starting one-d test\n");
  for (ii=0; ii < 3; ii++) yy[ii]= (double) ii;
  for (ii=0; ii < 3; ii++) xx[ii]= 1.0;
  diff= fabs(2.0-hoggLeastSquareFit(yy,xx,aa,3,1));
  printf("hoggTestLeastSquareFit: one-d chi-squared diff = %f\n",diff);
  if (diff > 1.0e-15) return 1;
  diff= fabs(1.0-aa[0]);
  printf("hoggTestLeastSquareFit: one-d parameter diff = %f\n",diff);
  if (diff > 1.0e-15) return 1;
  printf("hoggTestLeastSquareFit: starting two-d test\n");
  for (ii=0; ii < 3; ii++) xx[3+ii]= (double) ii;
  diff= fabs(0.0-hoggLeastSquareFit(yy,xx,aa,3,2));
  printf("hoggTestLeastSquareFit: two-d chi-squared diff = %f\n",diff);
  diff= fabs(1.0-aa[0]);
  if (diff > 1.0e-15) return 1;
  printf("hoggTestLeastSquareFit: first two-d parameter diff = %f\n",diff);
  diff= fabs(1.0-aa[1]);
  if (diff > 1.0e-15) return 1;
  printf("hoggTestLeastSquareFit: second two-d parameter diff = %f\n",diff);
  if (diff > 1.0e-15) return 1;
  return 0;
}
