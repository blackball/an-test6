/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <math.h>
#include <MathExt.h>
#include <BlockMemBank.hpp>
#include <stdlib.h>
#include <memory.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// 1.0 / ln(2)
#define  ILOG2         (1.4426950408889634073599246810019)

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// values used for calculating random floating point numbers
double IDOUBLE_RANDMAX = (double)1.0/(double)RAND_MAX;
float  IFLOAT_RANDMAX  = (float)1.0/(float)RAND_MAX;

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * log2: Returns the log-base-2 of the passed value.
 * Assumes: 'val' >= 0.
 */
double log2(double val)
{
  return log(val) * ILOG2;
}

/* -------------------------------------------------- */

/*
 * logb: Returns the log-base-b of the passed value 'val' in 
 *       base 'base'.
 * Assumes: 'val' >= 0, 'base' > 0.
 */
double logb(double val, double base)
{
  return log(val) / log(base);
}

/* -------------------------------------------------- */

/*
 * round: Rounds a double value to the nearest integer value.
 * Assumes: <none>
 */
double round(double val)
{
  return ceil(val-(double)0.5);
}

/* -------------------------------------------------- */

/*
 * round: Rounds a float value to the nearest integer value.
 * Assumes: <none>
 */
float round(float val)
{
  return (float)ceil(val-(float)0.5);
}

/* -------------------------------------------------- */

/*
 * mod: Returns the modulus of the double 'v1' to the double 'base'.
 * Assumes: <none>
 */
double mod(double v1, double base)
{
  return v1 - floor(v1 / base) * base;
}

/* -------------------------------------------------- */

/*
 * mod: Returns the modulus of the float 'v1' to the float 'base'.
 * Assumes: <none>
 */
float mod(float v1, float base)
{
  return v1 - (float)floor(v1 / base) * base;
}

/* -------------------------------------------------- */

/*
 * randD: Returns a uniform random double value in [0,1].
 * Assumes: <none>
 */
double randD()
{
  return (double)rand()*IDOUBLE_RANDMAX;
}

/* -------------------------------------------------- */

/*
 * randF: Returns a uniform random float value in [0,1].
 * Assumes: <none>
 */
float randF()
{
  return (float)rand()*IFLOAT_RANDMAX;
}

/* -------------------------------------------------- */

/*
 * randVD: Returns a vector of 'cnt' uniform random double values in [0,1], that have been
 *         multiplied by 'mult' and then had 'add' added to them. This is a good
 *         way to create uniform random values in a particular range.
 * Assumes: 'dest' can hold all 'cnt' random values.
 */
void randVD(double* dest, DWORD cnt, double mult, double add)
{
  DWORD i;
  
  for (i = 0; i < cnt; i++)
  {
    *(dest++) = ((double)rand()*IDOUBLE_RANDMAX) * mult + add;
  }
}

/* -------------------------------------------------- */

/*
 * randVDN: Returns a vector of 'cnt' gaussian random double values with standard 
 *          deviation 'stddev' and mean 'mean'.
 * Assumes: 'dest' can hold all 'cnt' random values.
 */
void randVDN(double* dest, DWORD cnt, double stddev, double mean)
{
  DWORD i;
  double r;
  double u[2], tv;

  for (i = 0; i < cnt; i++)
  {
    r = 2;
    while (r > 1)
    {
      u[0] = 2*randD()-1;
      u[1] = 2*randD()-1;
      r = u[0]*u[0]+u[1]*u[1];
    }
    tv = sqrt(-2*log(r)/r)*u[0];
    dest[i] = tv*stddev + mean;
  }
}

/* -------------------------------------------------- */

/*
 * randDN: Returns a gaussian random double value with standard 
 *         deviation 'stddev' and mean 'mean'.
 * Assumes: <none>
 */
double randDN(double stddev, double mean)
{
  double r;
  double u[2], tv;

  r = 2;
  while (r > 1)
  {
    u[0] = 2*randD()-1;
    u[1] = 2*randD()-1;
    r = u[0]*u[0]+u[1]*u[1];
  }
  tv = sqrt(-2*log(r)/r)*u[0];
  return (tv*stddev + mean);
}

/* -------------------------------------------------- */

/*
 * randVF: Returns a vector of 'cnt' uniform random float values in [0,1], that have been
 *         multiplied by 'mult' and then had 'add' added to them. This is a good
 *         way to create uniform random values in a particular range.
 * Assumes: 'dest' can hold all 'cnt' random values.
 */
void randVF(float* dest, DWORD cnt, float mult, float add)
{
  DWORD i;
  
  for (i = 0; i < cnt; i++)
  {
    *(dest++) = ((float)rand()*IFLOAT_RANDMAX) * mult + add;
  }
}

/* -------------------------------------------------- */

/*
 * randSphereD: Generates a uniform random double value on the unit sphere and returns
 *              the two angles 0 <= theta <= 2PI and 0 <= phi <= PI.
 * Assumes: 'theta' and 'phi' are valid pointers.
 */
void randSphereD(double *theta, double *phi)
{
  double u, v;

  do
  {
    u = randD();
  } while (u >= 1);
  v = randD();

  *theta  = 2*PI*u;
  *phi    = acos(2*v-1);
}

/* -------------------------------------------------- */

/*
 * randSphereD: Generated a uniform random double value on the unit sphere and returns
 *              the 3-vector of its position in 'dest'.
 * Assumes: 'dest' is a valid pointer to a 3-vector.
 */
void randSphereD(double *dest)
{
  double theta, phi;

  randSphereD(&theta, &phi);

  dest[0] = cos(theta)*sin(phi);
  dest[1] = sin(theta)*sin(phi);
  dest[2] = cos(phi);
}

/* -------------------------------------------------- */

/*
 * randSphereF: Generates a uniform random float value on the unit sphere and returns
 *              the two angles 0 <= theta <= 2PI and 0 <= phi <= PI.
 * Assumes: 'theta' and 'phi' are valid pointers.
 */
void randSphereF(float *theta, float *phi)
{
  float u, v;

  do
  {
    u = randF();
  } while (u >= 1);
  v = randF();

  *theta = (float) (2.0*PI*u);
  *phi   = (float) acos(2*v-1);
}

/* -------------------------------------------------- */

/*
 * randSphereF: Generated a uniform random double value on the unit sphere and returns
 *              the 3-vector of its position in 'dest'.
 * Assumes: 'dest' is a valid pointer to a 3-vector.
 */
void randSphereF(float *dest)
{
  float theta, phi;

  randSphereF(&theta, &phi);

  dest[0] = (float) (cos(theta)*sin(phi));
  dest[1] = (float) (sin(theta)*sin(phi));
  dest[2] = (float) (cos(phi));
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * factorial: Calculates the factorial of the positive integer 'x'.
 * Assumes: <none>
 */
double factorial(DWORD x)
{
  double res;
  double dx;

  dx = (double)x;
  res = 1;
  while (dx > 1)
  {
    res *= dx;
    dx--;
  }

  return res;
}

/* -------------------------------------------------- */

/*
 * choose: Calculates the value 'n' choose 'k'. The calculation is down step-wise
 *         rather than simply using factorial values in order to avoid overflows or underflows.
 *         The result is slower but safer.
 * Assumes: <none>
 */
double choose(DWORD n, DWORD k)
{
  double dn, dlarge, dsmall;
  double res;

  // return on bad params
  if (k > n)
    return 0;

  // return when no work needed
  if (((n == 0) || (k == 0)) || ((n-k) == 0))
    return 1;

  // determine the various limits of the factorials
  // and partial factorials to calculate
  dn = n;
  if (k > (n-k))
  {
    dlarge = k;
    dsmall = n-k;
  } else
  {
    dlarge = n-k;
    dsmall = k;
  }

  // calculate the choose value
  // calculating this way (stepwise) requires many divisions,
  // however it is much safer from overflow/underflow
  res = 1;
  while (dsmall > 1)
  {
    res *= dn--;
    res /= dsmall--;
  }
  res *= dn;
  
  return res;
}

/* -------------------------------------------------- */

/*
 * poisspdf: Generates the probability of the occurence 'x' in a poisson distribution according to
 *           'lambda'.
 * Assumes: lambda >= 0.
 */
double poisspdf(DWORD x, double lambda)
{
  double res;
  double dx;

  res = 1;
  if (lambda > 1)
    res *= exp(-lambda);
  if (x > 0)
  {
    // Calculate stepwise to avoid underflow/overflow. Slower but safer.
    dx = x;
    while (dx >= 1)
    {
      res *= lambda;
      res /= dx;
      dx--;
    }
  }
  if (lambda <= 1)
    res *= exp(-lambda);

  //return exp(log(pow(lambda, (double)x)) - log(factorial(x)) - lambda);  
  return res;
}

/* -------------------------------------------------- */

/*
 * sumPoiss: Returns the sum of the probabilities of the values in [0,max] for the
 *           poisson distribution with parameter 'lambda'.
 * Assumes: lambda >= 0.
 */
double sumPoiss(DWORD max, double lambda)
{
  double sum;
  DWORD v;

  v   = 0;
  sum = 0;
  for (v = 0; v <= max; v++)
  {
    sum += poisspdf(v, lambda);
  }

  return sum;
}

/* -------------------------------------------------- */

/*
 * sumUpPoiss: Returns the sum of the probabilities of the values in [min,inf] for the
 *             poisson distribution with parameter 'lambda' until the probability of 
 *             any particular value is smaller than 'acc'. The resulting probabilities for
 *             each value are returned in 'vals' which has space allocated for it via 'new',
 *             and the number of elements in 'vals' is returned via 'maxval'. The user will need
 *             to delete 'vals' themself.
 * Assumes: 'maxval' is valid, 'vals' is valid, lambda >= 0.
 */
double sumUpPoiss(DWORD *maxval, double **vals, DWORD min, double lambda, double acc)
{
  BlockMemBank tbank;
  double sum, tval;
  DWORD v, i, s;
  BYTE *bptr;
  Enumeration *tenum;

  // calculate each value
  tbank.Init(sizeof(double), 50);
  v = min;
  sum = 0;
  do
  {
    tval = poisspdf(v, lambda);
    tbank.AddNewData(&tval);
    sum += tval;
    v++;
  } while (tval > acc);

  // build the final required space
  s = tbank.GetCount();
  *vals = new double[s];
  *maxval = v-1;

  // copy the individiual values back into the output array
  tenum = tbank.NewEnum(FORWARD);
  i = 0;
  while (tenum->GetEnumOK() == TRUE32)
  {
    bptr = (BYTE*)tenum->GetEnumNext();
    (*vals)[i] = *((double*)bptr);
    i++;
  }
  delete tenum;
  tbank.Deinit();

  return sum;
}

/* -------------------------------------------------- */

/*
 * geopdf: Returns the probability of 'x' given the geometric distribution 
 *         with parameter 'p' in [0,1]
 * Assumes: p in [0,1].
 */
double geopdf(DWORD x, double p)
{
  return p*pow((1-p), (double)x);
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * luDCmp: From 'Numerical Recipes in C'. Creates LU decomposition of matrix A
 *         a    == 2D matrix in question
 *         n    == n x n dimension of matrix a
 *         indx == permutation indicator
 *         d    == +/- 1 indicating row permutation count as even/odd
 *         Returns FALSE32 if matrix is singular, TRUE32 otherwise.
 * Call using
 *    luDCmp(A, 3, indx, &d);
 *    luBackSub(A, 3, indx, b);
 *    By using luDCmp you can solve multiple b's using luBackSub within recalling
 *    luDCmp.
 * Assumes: 'a' and 'indx' are appropriately sized to 'n', 'd' is a valid double pointer.
 */
BOOL32 luDCmp(double *a, int n, int *indx, double *d)
{
  int i,imax,j,k;
  double big,dum,sum,temp;
  double *vv; // vv stores the implicit scaling of each row.
  
  vv = new double[n];
  *d = 1.0; // No row interchanges yet.

  //Loop over rows to get the implicit scaling information.
  for (i = 0; i < n; i++) 
  {
    big = 0.0;
    for (j = 0; j < n; j++)
    {
      if ((temp = ABS(a[i*n+j])) > big)
      {
        big = temp;
      }
    }
    if (big == 0.0)
      return FALSE32; // Singular matrix
    // No nonzero largest element
    vv[i] = 1.0/big; // Save the scaling.
  }

  //  This is the loop over columns of Crout’s method
  for (j = 0; j < n; j++) 
  {
    //  This is equation (2.3.12) except for i == j.
    for (i = 0; i < j; i++) 
    {
      sum = a[i*n+j];
      for (k = 0; k < i; k++) 
      {
        sum -= a[i*n+k]*a[k*n+j];
      }
      a[i*n+j] = sum;
    }
    big = 0.0; // Initialize for the search for largest pivot element.

     // This is i = j of equation (2.3.12) and i = j+1. . .N
     // of equation (2.3.13). sum=a[i][j];
    for (i = j; i < n; i++) 
    {
      sum = a[i*n+j];
      for (k = 0; k < j; k++)
      {
        sum -= a[i*n+k]*a[k*n+j];
      }
      a[i*n+j] = sum;
      if ( (dum = vv[i]*ABS(sum)) >= big) 
      {
        // Is the figure of merit for the pivot better than the best so far?
        big = dum;
        imax = i;
      }
    }
    // Do we need to interchange rows?
    if (j != imax)
    { 
      // Yes, do so...
      for (k = 0; k < n; k++)
      { 
        dum = a[imax*n+k];
        a[imax*n+k] = a[j*n+k];
        a[j*n+k] = dum;
      }
      *d = -(*d);     // ...and change the parity of d.
      vv[imax] = vv[j]; // Also interchange the scale factor.
    }
    indx[j] = imax;
    if (a[j*n+j] == 0.0) 
      a[j*n+j] = TINY;
    // If the pivot element is zero the matrix is singular (at least to the precision of the
    // algorithm). For some applications on singular matrices, it is desirable to substitute
    // TINY for zero.

    // Now, finally, divide by the pivot element.
    if (j != n)
    { 
      dum = 1.0/(a[j*n+j]);
      for (i = j+1; i < n; i++)
      {
        a[i*n+j] *= dum;
      }
    }
  } // Go back for the next column in the reduction.

  delete [] vv;

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * luDCmp: From 'Numerical Recipes in C'. Finishes solving A x == b
 *         a    == matrix 'a' resulting from luDCmp
 *         n    == n x n dimension of matrix a
 *         indx == indx vector returned from luDCmp
 *         b    == 'b' vecor in A x == b, filled with the result 'x'
 * Call using
 *    luDCmp(A, 3, indx, &d);
 *    luBackSub(A, 3, indx, b);
 * Assumes: 'a', 'b' and 'indx' are appropriately sized to 'n'.
 */
void luBackSub(double *a, int n, int *indx, double *b)
{
  int i, ii, ip, j;
  double sum;

  // When ii is set to a positive value, it will become the
  // index of the first nonvanishing element of b. We now
  // do the forward substitution, equation (2.3.6). The
  // only new wrinkle is to unscramble the permutation
  // as we go.
  ii = -1;
  for (i = 0; i < n; i++)
  {
    ip    = indx[i];
    sum   = b[ip];
    b[ip] = b[i];
    if (ii >= 0)
    {
      for (j = ii; j <= i-1; j++)
      {
        sum -= a[i*n+j]*b[j];
      }
    } else if (sum) 
    {
      // A nonzero element was encountered, so from now on we
      // will have to do the sums in the loop above. b[i]=sum;
      ii = i;
    }
    b[i] = sum;
  }

  // Now we do the backsubstitution, equation (2.3.7).
  for (i = n-1; i >= 0; i--)
  {
    sum = b[i];
    for (j = i+1; j < n; j++)
    {
      sum -= a[i*n+j]*b[j];
    }
    b[i] = sum/a[i*n+i]; // Store a component of the solution vector X.
  } // All done!
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
