#include <math.h>
#include <stdio.h>
#include "hoggTweak.h"
#include "hoggMath.h"
int hoggTestRansacTrial()
{
  int ii,use[5];
  double yy[5],xx[10],aa[2],shift=3.0,scale=2.0,val0,val1,diff;
  printf("hoggTestRansacTrial: starting one-d test\n");
  for (ii=0; ii < 5; ii++){
    xx[ii]= 1.0;
    xx[ii+5]= (double)ii;
    yy[ii]= shift*xx[ii];
    use[ii]= 0;
  }
  yy[0]+= 1.0;
  use[0]= 1;
  val0= hoggRansacTrial(yy,xx,aa,5,1,use);
  use[0]= 0;
  use[1]= 1;
  val1= hoggRansacTrial(yy,xx,aa,5,1,use);
  diff= val1-val0;
  printf("hoggTestRansacTrial: one-d scalar diff = %e\n",diff);
  if (diff <= 0.0) return 1;
  diff= fabs(scale-aa[0]);
  printf("hoggTestRansacTrial: one-d parameter diff = %e\n",diff);
  if (diff > 1.0e-15) return 1;
  printf("hoggTestRansacTrial: starting two-d test\n");
  for (ii=0; ii < 5; ii++){
    yy[ii]= shift*xx[ii]+scale*xx[ii+5];
    use[ii]= 0;
  }
  yy[0]+= 1.0;
  use[0]= 1;
  val0= hoggRansacTrial(yy,xx,aa,5,1,use);
  use[0]= 0;
  use[1]= 1;
  val1= hoggRansacTrial(yy,xx,aa,5,1,use);
  diff= val1-val0;
  printf("hoggTestRansacTrial: two-d scalar diff = %e\n",diff);
  if (diff <= 0.0) return 1;
  diff= fabs(shift-aa[0]);
  printf("hoggTestRansacTrial: two-d parameter diff = %e\n",diff);
  if (diff > 1.0e-15) return 1;
  diff= fabs(scale-aa[1]);
  printf("hoggTestRansacTrial: two-d parameter diff = %e\n",diff);
  if (diff > 1.0e-15) return 1;
  return 0;
}
