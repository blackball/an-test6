/* 
   File:         hrect.cpp
   Author:       Andrew W. Moore
   Created:      Jan 13th, 1998
   Description:  The representation of hyper rectangles.
*/

#include "hrect.h"

/* A hrect represents a hyperrectangle in k-dimensional space.
   This is easily represented as two vectors: the values of
   all the low coordinates and high coordinates. For example
   this 2-d rectangle

  y = 7        +-------+
               |       |
  y = 4        +-------+

               ^       ^
               x = -1  x = 3

   can be represented by lo = ( -1 , 4 )
                         hi = (  3 , 7 )
*/

/* makes a hyper rectangle with lowest value in ith dimension
   dyv_ref(lo,i) and highest value dyv_ref(hi,i) */
hrect *mk_hrect(const dyv *lo, const dyv *hi)
{
  hrect *hr = AM_MALLOC(hrect);
  hr -> lo = mk_copy_dyv(lo);
  hr -> hi = mk_copy_dyv(hi);

  my_assert( dyv_size(hr->lo) == dyv_size(hr->hi) );

  return hr;
}

void copy_hrect(hrect *src,hrect *dest)
{
  copy_dyv(src->lo,dest->lo);
  copy_dyv(src->hi,dest->hi);
}

hrect *mk_copy_hrect(const hrect *hr)
{
  return mk_hrect(hr->lo,hr->hi);
}

/* Parses the string, which should be the ASCII form of a set
   of space separated numbers, eg "0.8 99 -5e4 876" (which would
   generate a dyv of size 4). Raises a my_error if cannot be
   parsed in this way. String must be null (\0) terminated. */
dyv *mk_dyv_from_string_simple(char *s)
{
  dyv *x = mk_dyv_from_string(s,NULL);
  if ( x == NULL )
  {
    printf("Can't parse this as a vector: %s\n",s);
    my_error("mk_dyv_from_string_simple");
  }
  return x;
}

/* This parses two strings (which must be space-separated ASCII
   representations of numbers). It turns the first into the low
   vector for the hyperrectnagle and the second into the high vector.
   Clearly both vectors must be the same length (i.e. have the same
   number of numbers). The number of numbers is the dimensionality of
   the hyper-rectangle */
hrect *mk_hrect_from_strings(char *lo_string,char *hi_string)
{
  dyv *lo = mk_dyv_from_string_simple(lo_string);
  dyv *hi = mk_dyv_from_string_simple(hi_string);
  hrect *hr = mk_hrect(lo,hi);

  free_dyv(lo);
  free_dyv(hi);

  return hr;
}

/* The dimensionality of the hyper-rectangle */
int hrect_size(const hrect *hr)
{
  return dyv_size(hr->lo);
}

/* MAKES and returns the center of the hyper-rectangle. (This is
   of course just the mean of lo and hi) */
dyv *mk_hrect_middle(const hrect *hr)
{
  dyv *sum = mk_dyv_plus(hr->lo,hr->hi);
  dyv_scalar_mult(sum,0.5,sum);
  return sum;
}

/* If x is inside the hyper-rectangle it is unchanged.
   If x is outside the hyper-rectangle it is changed to
   become the closest point to x that is in (or on the surface
   of) the hyper-rectangle */
void clip_to_inside_hrect(const hrect *hr, dyv *x)
{
  int i;
  for ( i = 0 ; i < dyv_size(x) ; i++ )
  {
    double xi = dyv_ref(x,i);
    if ( xi < dyv_ref(hr->lo,i) )
      dyv_set(x,i,dyv_ref(hr->lo,i));
    else if ( xi > dyv_ref(hr->hi,i) )
      dyv_set(x,i,dyv_ref(hr->hi,i));
  }
}

/* Returns TRUE if and only if query_point is inside or on
   the edge of the hyper-rectangle */
bool is_in_hrect(const hrect *hyp, const dyv *query_point)
{
  return dyv_weakly_dominates(hyp->hi,query_point) &&
         dyv_weakly_dominates(query_point,hyp->lo);
}

int first_outlying_dim(const hrect *hyp, const dyv *query_point)
{
  int d;

  if (hrect_size(hyp) != dyv_size(query_point)) {
    my_error("first_outlying_dim: dyvs (DYnamic Vectors) different shape");
  }

  for (d=0; d < dyv_size(query_point) ; d++ ) {
    if(hrect_lo_ref(hyp, d) > dyv_ref(query_point, d) ||
       hrect_hi_ref(hyp, d) < dyv_ref(query_point, d))
      return d;
  }
  return -1;
}

/* am_frees the hyper-rect. (And, in accordance with AM conventions,
   it frees all the substructures). */
void free_hrect(hrect *hyp)
{
  free_dyv(hyp->lo);
  free_dyv(hyp->hi);
  AM_FREE(hyp,hrect);
}

/* Prints using the AM-FPRINTF-CONVENTION */
void fprintf_hrect(FILE *s,char *m1,hrect *hyp,char *m2)
{
  char buff[500];
  sprintf(buff,"%s -> lo",m1);
  fprintf_dyv(s,buff,hyp->lo,m2);
  sprintf(buff,"%s -> hi",m1);
  fprintf_dyv(s,buff,hyp->hi,m2);
}

void phrect(hrect *x)
{
  fprintf_hrect(stdout,"hr",x,"\n");
}

/* rows may be NULL denoting "use all rows" */
hrect *mk_hrect_bounding_dyv_array_rows(const dyv_array *da,ivec *rows)
{
  dyv *lo,*hi;
  hrect *hr;
  make_vector_limits_from_dyv_array(da,rows,&lo,&hi);
  hr = mk_hrect(lo,hi);
  free_dyv(lo);
  free_dyv(hi);
  return hr;
}

hrect *mk_hrect_bounding_dyv_array(const dyv_array *da)
{
  return mk_hrect_bounding_dyv_array_rows(da,NULL);
}

int hrect_widest_dim(const hrect *hr)
{
  int result = -1;
  double wmax = -1.0;
  int i;
  for ( i = 0 ; i < hrect_size(hr) ; i++ )
  {  
    double w = hrect_width_ref(hr,i);
    if ( i == 0 || w > wmax )
    {
      wmax = w;
      result = i;
    }
  }
  return result;
}

double hrect_middle_ref(const hrect *hr, int index)
{
  return 0.5 * ( dyv_ref(hr->hi,index) + dyv_ref(hr->lo,index) );
}

double hrect_width_ref(const hrect *hr, int index)
{
  return dyv_ref(hr->hi,index) - dyv_ref(hr->lo,index);
}

/* x_minlik and x_maxlik may be NULL.

 *distmin is set to be min_x_in_hr( [x - center]^2 )
 *distmax is set to be max_x_in_hr( [x - center]^2 )

   if x_minlik is non-null, it gets filled with 
     argmax_x_in_hr ( [x - center]^2 )
   if x_maxlik is non-null, it gets filled with 
     argmin_x_in_hr ( [x - center]^2 )

  Note that "x_minlik" means the point least likely to have been created
  by center, which is why it is defined as an argmax.
*/
void closest_and_furthest_hrect_distance(hrect *hr,dyv *center,
                                         double *distmin,double *distmax,
                                         dyv *x_minlik,dyv *x_maxlik)
{
  int i;
  double sum_max_dsqd = 0.0;
  double sum_min_dsqd = 0.0;

  for ( i = 0 ; i < dyv_size(center) ; i++ )
  {
    double lo = hrect_lo_ref(hr,i);
    double hi = hrect_hi_ref(hr,i);
    double x = dyv_ref(center,i);
    double dmin,dmax;

    if ( x <= lo )
    {
      dmin = lo - x;
      dmax = hi - x;
      if ( x_minlik != NULL ) dyv_set(x_minlik,i,hi);
      if ( x_maxlik != NULL ) dyv_set(x_maxlik,i,lo);
    }
    else if ( x >= hi )
    {
      dmin = x - hi;
      dmax = x - lo;
      if ( x_minlik != NULL ) dyv_set(x_minlik,i,lo);
      if ( x_maxlik != NULL ) dyv_set(x_maxlik,i,hi);
    }
    else
    {
      dmin = 0.0;
      dmax = ( x < (lo+hi)/2.0 ) ? hi-x : x-lo;
      if ( x_minlik != NULL ) dyv_set(x_minlik,i,(x<(lo+hi)/2.0) ? hi : lo);
      if ( x_maxlik != NULL ) dyv_set(x_maxlik,i,x);
    }

    sum_max_dsqd += dmax * dmax;
    sum_min_dsqd += dmin * dmin;
  }
 
  *distmax = sqrt(sum_max_dsqd);
  *distmin = sqrt(sum_min_dsqd);
}

/* Returns a new hrect translated by subtracting the vector
   x from the lower limits and the higher limits of hr */
hrect *mk_hrect_subtract_dyv(hrect *hr,dyv *x)
{
  hrect *hr_trans = mk_copy_hrect(hr);
  dyv_subtract(hr->lo,x,hr_trans->lo);
  dyv_subtract(hr->hi,x,hr_trans->hi);
  return hr_trans;
}

hrect *mk_zero_hrect(int size)
{
  dyv *zeroes = mk_zero_dyv(size);
  hrect *hr = mk_hrect(zeroes,zeroes);
  free_dyv(zeroes);
  return hr;
}

/* Makes a slightly larger hrect that encloses hr, but has "sensible" (i.e. round)
   numbers at the bootom and top of all its dimensions */
hrect *mk_hrect_sensible_limits(hrect *hr)
{
  hrect *newhr = mk_copy_hrect(hr);
  set_vector_limits_sensible(newhr->lo,newhr->hi);
  return newhr;
}

bool hrect_equal(hrect *hr1,hrect *hr2)
{
  return dyv_equal(hr1->lo,hr2->lo) &&
         dyv_equal(hr1->lo,hr2->lo);
}

/* Generates a vector randomly uniformly from hrect */
dyv *mk_sample_from_hrect(const hrect *hr)
{
  int size = hrect_size(hr);
  int i;
  dyv *x = mk_dyv(size);
  for ( i = 0 ; i < size ; i++ )
    dyv_set(x,i,range_random(hrect_lo_ref(hr,i),hrect_hi_ref(hr,i)));
  return x;
}

/* Returns the area (volume?) of the hrect, i.e. the product of all
   its widths. */
double hrect_area(const hrect *hr)
{
  /* go through each dimension, extract the values of the low and high
     points in that dimension, calculate d_low - d_high) and multiply
     that by the current area calculation */
  double area = 1.0;
  int i;
  for (i = 0; i < hrect_size(hr); i++) 
  {
    double lo = hrect_lo_ref(hr,i);
    double hi = hrect_hi_ref(hr,i);
    area *= hi - lo;
  }

  return area;
}

dyv *mk_hrect_width(const hrect *hr)
{
  dyv *ret = mk_copy_dyv(hr->hi);
  dyv_subtract(ret, hr->lo, ret);
  return ret;
}

bool hrects_intersect(hrect *a,hrect *b)
{
  bool result = TRUE;
  int i;
  for ( i = 0 ; result && i < hrect_size(a) ; i++ )
  {
    double alo = hrect_lo_ref(a,i);
    double ahi = hrect_hi_ref(a,i);
    double blo = hrect_lo_ref(b,i);
    double bhi = hrect_hi_ref(b,i);
    if ( ahi <= blo || bhi <= alo )
      result = FALSE;
  }
  return result;
}

/* Returns TRUE iff a is inside b */
bool hrect_is_inside(hrect *a,hrect *b)
{
  bool result = TRUE;
  int i;
  for ( i = 0 ; result && i < hrect_size(a) ; i++ )
  {
    double alo = hrect_lo_ref(a,i);
    double ahi = hrect_hi_ref(a,i);
    double blo = hrect_lo_ref(b,i);
    double bhi = hrect_hi_ref(b,i);
    if ( alo < blo || ahi > bhi )
      result = FALSE;
  }
  return result;
}

hrect *mk_hrect_1(double lo0,double hi0)
{
  dyv *xlo = mk_dyv_1(lo0);
  dyv *xhi = mk_dyv_1(hi0);
  hrect *hr = mk_hrect(xlo,xhi);
  free_dyv(xlo);
  free_dyv(xhi);
  return hr;
}

/* Makes the smallest hrect that contains both
   (x1,y1) and (x2,y2) */
hrect *mk_hrect_2(double x1,double y1,double x2,double y2)
{
  dyv *lo = mk_dyv_2(real_min(x1,x2),real_min(y1,y2));
  dyv *hi = mk_dyv_2(real_max(x1,x2),real_max(y1,y2));
  hrect *hr = mk_hrect(lo,hi);
  free_dyv(lo);
  free_dyv(hi);
  return hr;
}

/* Make a smaller hrect from a subset of the bounds of the original
   hrect.  For example, if the original hrect had the following
   hi and lo dyvs:
      lo: "0 0 0 0"
      hi: "1 2 3 4"
   And we wanted to make a hrect with the first two dimensions ie.
      lo: "0 0"
      hi: "1 2"
   We would call mk_subhrect with the dims ivec = "0 1"
   */
hrect *mk_subhrect(hrect* hr, ivec* dims)
{
  int i;
  dyv* lo;
  dyv* hi;
  hrect* result;

  lo = mk_dyv(ivec_size(dims));
  hi = mk_dyv(ivec_size(dims));
  
  for( i = 0; i < ivec_size(dims); i++ )
    {
      dyv_set(lo,i,hrect_lo_ref(hr,ivec_ref(dims,i)));
      dyv_set(hi,i,hrect_hi_ref(hr,ivec_ref(dims,i)));
    }
  result = mk_hrect(lo,hi);
  free_dyv(lo);
  free_dyv(hi);
  return result;
}

/* Makes a new hrect in which each dimension has the same width as the
   old one but in which the center is moved to new_center */
hrect *mk_recenter_hrect(hrect *hr,dyv *new_center)
{
  hrect *new_hr = mk_copy_hrect(hr);
  int i;
  for ( i = 0 ; i < hrect_size(hr) ; i++ )
  {
    double width = hrect_width_ref(hr,i);
    double center = dyv_ref(new_center,i);
    hrect_lo_set(new_hr,i,center - width/2.0);
    hrect_hi_set(new_hr,i,center + width/2.0);
  }
  return new_hr;
}

/* Returns a new hrect with the same center as hrect but in which all
   widths have been multiplied by a factor of scale. */
hrect *mk_scale_hrect(hrect *hr,double scale)
{
  hrect *new_hr = mk_copy_hrect(hr);
  int i;
  for ( i = 0 ; i < hrect_size(hr) ; i++ )
  {
    double width = hrect_width_ref(hr,i) * scale;
    double center = hrect_middle_ref(hr,i);
    hrect_lo_set(new_hr,i,center - width/2.0);
    hrect_hi_set(new_hr,i,center + width/2.0);
  }
  return new_hr;
}

double hrect_diameter(hrect* hr)
{
  return(sqrt(dyv_dsqd(hr->lo,hr->hi)));
}

/* Makes an hrect whose center is middle and whose i'th dimension
   has width widths[i] */
hrect *mk_hrect_from_middle_and_widths(dyv *middle,dyv *widths)
{
  dyv *lo = mk_dyv_scalar_combine(middle,widths,1.0,-0.5);
  dyv *hi = mk_dyv_scalar_combine(middle,widths,1.0,0.5);
  hrect *result = mk_hrect(lo,hi);
  free_dyv(lo);
  free_dyv(hi);
  return result;
}

/* Makes the smallest cubic (i.e. all widths same) hrect that
   surrounds hr entirely */
hrect *mk_cube_hrect(dyv *middle,double width)
{
  dyv *widths = mk_constant_dyv(dyv_size(middle),width);
  hrect *result = mk_hrect_from_middle_and_widths(middle,widths);
  free_dyv(widths);
  return result;
}

/* Makes the smallest cubic (i.e. all widths same) hrect that
   surrounds hr entirely */
hrect *mk_smallest_surrounding_cube_hrect(hrect *hr)
{
  double w = hrect_width_ref(hr,hrect_widest_dim(hr));
  dyv *middle = mk_hrect_middle(hr);
  hrect *result = mk_cube_hrect(middle,w);
  free_dyv(middle);
  return result;
}
  
hrect *mk_unit_hrect(int size)
{
  dyv *lo = mk_constant_dyv(size,0.0);
  dyv *hi = mk_constant_dyv(size,1.0);
  hrect *hr = mk_hrect(lo,hi);
  free_dyv(lo);
  free_dyv(hi);
  return hr;
}

/* If x is inside hr then hr is unchanged.

   Else hr is grown to become the smallest hrect
   that contains both the original hrect and x */
void maybe_expand_hrect(hrect *hr,dyv *x)
{
  int i;
  for ( i = 0 ; i < hrect_size(hr) ; i++ )
  {
    double lo = hrect_lo_ref(hr,i);
    double hi = hrect_hi_ref(hr,i);
    double val = dyv_ref(x,i);
    if ( val < lo )
      hrect_lo_set(hr,i,val);
    else if ( val > hi )
      hrect_hi_set(hr,i,val);
  }
}

/* A "cubic_hrect" is one in which all sides are the same
   length.

   This function makes an hrect centered on middle in which each
   side is of length "width" */
hrect *mk_cubic_hrect_with_middle_and_width(dyv *middle,double width)
{
  int size = dyv_size(middle);
  dyv *width_vec = mk_constant_dyv(size,width/2);
  dyv *lo = mk_dyv_subtract(middle,width_vec);
  dyv *hi = mk_dyv_plus(middle,width_vec);
  hrect *hr = mk_hrect(lo,hi);
  free_dyv(width_vec);
  free_dyv(lo);
  free_dyv(hi);
  return hr;
}

/* A "cubic_hrect" is one in which all sides are the same
   length.
   This function makes the smallest cubic hrect that contains hr.
   It is centered on the center of hr */
hrect *mk_cubic_hrect_surrounding_hrect(hrect *hr)
{
  int wd = hrect_widest_dim(hr);
  double width = hrect_width_ref(hr,wd);
  dyv *middle = mk_hrect_middle(hr);
  hrect *result = mk_cubic_hrect_with_middle_and_width(middle,width);
  free_dyv(middle);
  return result;
}

/* Makes the smallest hrect that contains all the dyvs in sps.

   PRE: All dyvs in sps must be the same length */
hrect *mk_hrect_surrounding_dyv_array(dyv_array *sps)
{
  int num_rows = dyv_array_size(sps);
  hrect *hr = NULL;

  if ( num_rows < 1 )
    my_error("mk_hrect_surrounding_dyv_array");
  else
  {
    dyv *x_0 = dyv_array_ref(sps,0);
    int i;

    hr = mk_hrect(x_0,x_0);

    for ( i = 1 ; i < num_rows ; i++ )
    {
      dyv *x = dyv_array_ref(sps,i);
      maybe_expand_hrect(hr,x);
    }
  }

  return hr;

  
}

/* A "cubic_hrect" is one in which all sides are the same
   length.
   Makes the smallest cubic hrect that contains all the dyvs in sps.

   PRE: All dyvs in sps must be the same length */
hrect *mk_cubic_hrect_surrounding_dyv_array(dyv_array *sps)
{
  hrect *hr = mk_hrect_surrounding_dyv_array(sps);
  hrect *result = mk_cubic_hrect_surrounding_hrect(hr);
  free_hrect(hr);
  return result;
}

