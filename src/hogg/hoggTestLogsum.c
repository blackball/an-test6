#include <math.h>
#include <stdio.h>
#include "hoggTweak.h"
int hoggTestLogsum()
{
  int ii;
  double list[4],diff;
  printf("hoggTestLogsum: testing sum\n");
  list[0]= 0.0;
  list[1]= log(0.001);
  list[2]= log(0.000001);
  list[3]= log(0.000000001);
  diff= fabs(log(1.001001001)-hoggLogsum(list,4));
  printf("hoggTestLogsum: got diff = %lf\n",diff);
  if (diff > 1e-16) return 1;
  return 0;
}
