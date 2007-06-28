#include <math.h>
#include "hoggMath.h"
double hoggLogGaussian(double xx,double var)
{
  return (-0.5*log(2.0*PI*var)-0.5*xx*xx/var);
}
