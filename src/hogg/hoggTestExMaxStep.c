#include <stdio.h>
#include <math.h>
#include "hoggTweak.h"
int hoggTestExMaxStep()
{
  int ii;
  double like1, like2, diff, xx[4], varin[4], varout, logpin;
  printf("hoggTestExMaxStep: testing improvement in log like\n");
  xx[0]= 0.1;
  xx[1]= -0.2;
  xx[2]= 1.0;
  xx[3]= 2.0;
  varin[0]= 0.01;
  varin[1]= 0.02;
  varin[2]= 0.02;
  varin[3]= 0.01;
  varout= 4.0;
  logpin= log(0.5);
  like1= hoggExMaxStep(xx,varin,4,varout,&logpin);
  like2= hoggExMaxStep(xx,varin,4,varout,&logpin);
  diff= like2-like1;
  printf("hoggTestExMaxStep: first iteration diff = %lf\n",diff);
  if (diff <= 0.0) return 1;
  like1= like2;
  for (ii=0;ii<100;ii++) like2= hoggExMaxStep(xx,varin,4,varout,&logpin);
  diff= like2-like1;
  printf("hoggTestExMaxStep: next 100 iterations diff = %lf\n",diff);
  if (diff <= 0.0) return 1;
  like1= like2;
  like2= hoggExMaxStep(xx,varin,4,varout,&logpin);
  diff= like2-like1;
  printf("hoggTestExMaxStep: 101st iteration diff = %lf\n",diff);
  if (diff < 0.0) return 1;
  return 0;
}
