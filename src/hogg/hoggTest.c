#include <stdio.h>
#include "hoggTweak.h"

int hoggTestLogsum();

int main()
{
  int status;
  status= hoggTestLogsum();
  if (status != 0) return(status);
  status= hoggTestLogGaussian();
  if (status != 0) return(status);
  status= hoggTestExMaxStep();
  if (status != 0) return(status);
  status= hoggTestLeastSquareFit();
  if (status != 0) return(status);
  printf("hoggTest: all tests passed.\n");
  return 0;
}
