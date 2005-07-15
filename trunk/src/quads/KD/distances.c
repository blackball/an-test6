/*
   File:        distances.c
   Author:      Andrew W. Moore
   Created:     June 2000
   Description: Utilities for measuring metric distances between points
                and rectangles.

   Copyright 2000, Schenley Park Research
*/

#include "distances.h"

#define MIN_DIST 0
#define MAX_DIST 1

/* dist_type MUST be one of MIN_DIST, MAX_DIST.
   if it's MIN_DIST function returns the shortest possible squared distance between
   two points: one in a and the other in b. Result is thus 0 if the hrects overlap.
   if it's MAX_DIST function returns the largest possible squared distance between
   two points: one in a and the other in b. 
*/
double hrect_hrect_dsqd(hrect *a,hrect *b,int dist_type)
{
  double result = 0.0;
  int i;
  int size = hrect_size(a);

  my_assert(size == hrect_size(b));

  for ( i = 0 ; i < size ; i++ )
  {
    double delta;
    double alo = hrect_lo_ref(a,i);
    double ahi = hrect_hi_ref(a,i);
    double blo = hrect_lo_ref(b,i);
    double bhi = hrect_hi_ref(b,i);
    if ( dist_type == MIN_DIST )
    {
      if ( ahi < blo )
        delta = blo - ahi;
      else if ( bhi < alo )
        delta = alo - bhi;
      else
        delta = 0.0;
    }
    else
    {
      my_assert(dist_type == MAX_DIST);
      delta = real_max(ahi-blo,bhi-alo);
    }

    result += delta * delta;
  }

  return result;
}

/* function returns the shortest possible squared distance between
   two points: one in a and the other in b. Result is thus 0 if the hrects overlap.
*/
double hrect_hrect_min_dsqd(hrect *a,hrect *b)
{
  return hrect_hrect_dsqd(a,b,MIN_DIST);
}

/* function returns the largest possible squared distance between
   two points: one in a and the other in b. 
*/
double hrect_hrect_max_dsqd(hrect *a,hrect *b)
{
  return hrect_hrect_dsqd(a,b,MAX_DIST);
}

/* Returns the squared distance between x1 and x2 */
double dyv_dyv_dsqd(dyv *x1,dyv *x2)
{
  double result = 0.0;
  int i;

  my_assert(dyv_size(x1) == dyv_size(x2));

  for ( i = dyv_size(x1)-1 ; i >= 0 ; i-- )
  {
    double d = dyv_ref(x1,i) - dyv_ref(x2,i);
    result += d * d;
  }
  return result;
}

/* Returns the squared distance between x and a point in hr.

   if dist_type == MIN_DIST uses the closest point in hr to x
   if dist_type == MAX_DIST uses the furthest point in hr to x
*/
double hrect_dyv_dsqd(hrect *hr,dyv *x,int dist_type)
{
  double result = 0.0;
  int i;
  int size = hrect_size(hr);

  my_assert(size == dyv_size(x));

  for ( i = 0 ; i < size ; i++ )
  {
    double delta;
    double alo = hrect_lo_ref(hr,i);
    double ahi = hrect_hi_ref(hr,i);
    double b = dyv_ref(x,i);

    if ( dist_type == MIN_DIST )
    {
      if ( ahi < b )
        delta = b - ahi;
      else if ( b < alo )
        delta = alo - b;
      else
        delta = 0.0;
    }
    else
    {
      my_assert(dist_type == MAX_DIST);
      delta = real_max(ahi-b,b-alo);
    }

    result += delta * delta;
  }

  return result;
}

/* Returns the squared distance between x and the closest point in hr.
   If x is in hr the result is thus zero.
*/
double hrect_dyv_min_dsqd(hrect *hr,dyv *x)
{
  return hrect_dyv_dsqd(hr,x,MIN_DIST);
}

/* Returns the squared distance between x and the furthest point in hr. */
double hrect_dyv_max_dsqd(hrect *hr,dyv *x)
{
  return hrect_dyv_dsqd(hr,x,MAX_DIST);
}

