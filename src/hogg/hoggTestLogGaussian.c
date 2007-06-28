#include <math.h>
#include <stdio.h>
#include "hoggTweak.h"
#include "hoggMath.h"
int hoggTestLogGaussian()
{
  double diff;
  printf("hoggTestLogGaussian: testing value\n");
  diff= fabs(-PI-hoggLogGaussian(1.0,0.5/PI));
  printf("hoggTestLogGaussian: got diff = %lf\n",diff);
  if (diff > 1.0e-16) return 1;
  printf("hoggTestLogGaussian: testing x scaling\n");
  diff= fabs(3.0-hoggLogGaussian(1.0,0.5)+hoggLogGaussian(2.0,0.5));
  printf("hoggTestLogGaussian: got diff = %lf\n",diff);
  if (diff > 1.0e-16) return 1;
  printf("hoggTestLogGaussian: testing var scaling\n");
  diff= fabs(log(2.0)-hoggLogGaussian(0.0,1.0)+hoggLogGaussian(0.0,4.0));
  printf("hoggTestLogGaussian: got diff = %lf\n",diff);
  if (diff > 1.0e-16) return 1;
  return 0;
}
