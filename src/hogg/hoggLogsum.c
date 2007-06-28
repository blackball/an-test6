#include <math.h>
double hoggLogsum(double * list,int nn)
{
  int ii;
  double maxl,sum;
  maxl= list[0];
  for (ii=1;ii<nn;ii++) maxl= (maxl > list[ii]) ? maxl : list[ii];
  sum= 0.0;
  for (ii=0;ii<nn;ii++) sum+= exp(list[ii]-maxl);
  return (log(sum)+maxl);
}
