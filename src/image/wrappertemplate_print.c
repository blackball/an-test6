#include <math.h>
#include <stdio.h>
#include <string.h>

void wrappertemplate_printit(float *data,
                           int nx,
                           int ny)
{
  int i,j;

  for(i=0;i<nx;i++)
    for(j=0;j<ny;j++)
      printf("%d %d %e\n", i,j, data[j*nx+i]);
  for(i=0;i<nx;i++)
    for(j=0;j<ny;j++)
      data[j*nx+i]=data[j*nx+i]+1.;
  for(i=0;i<nx;i++)
    for(j=0;j<ny;j++)
      printf("%d %d %e\n", i,j, data[j*nx+i]);
  
}
