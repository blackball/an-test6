#include <math.h>
int iseed = 4;

double ranf()
/* Uniform random number generator x(n+1)= a*x(n) mod c
   with a = pow(7,5) and c = pow(2,31)-1.
   Copyright (c) Tao Pang 1997. */
{
	const int ia=16807,ic=2147483647,iq=127773,ir=2836;
	int il,ih,it;
	double rc;
	ih = iseed/iq;
	il = iseed%iq;
	it = ia*il-ir*ih;
	if (it > 0)
		iseed = it;
	else
		iseed = ic+it;
	rc = ic;
	return iseed/rc;
}

void grnf (double *x, double *y)
/* Two Gaussian random numbers generated from two uniform
   random numbers.  Copyright (c) Tao Pang 1997. */
{
	double pi,r1,r2;

	pi =  4*atan(1);
	r1 = -log(1-ranf());
	r2 =  2*pi*ranf();
	r1 =  sqrt(2*r1);
	*x  = r1*cos(r2);
	*y  = r1*sin(r2);
}

