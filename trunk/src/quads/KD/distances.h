/*
   File:        distances.h
   Author:      Andrew W. Moore
   Created:     June 2000
   Description: Utilities for measuring metric distances between points
                and rectangles.

   Copyright 2000, Schenley Park Research
*/

#ifndef DISTANCES_H
#define DISTANCES_H

/* Read first: hrect.h (representing hyperrectangles in k-dimensional space)

   This file contains APIs for computing distances between points in k-d
   space and minimum and maximum point/hyperrectangle distances and 
   hyperrectangle/hyperrectangle distances.

   All return values are in fact squared distances (a squared distance is
   known colloquially as a "dsqd") to save the usually unnecessary cost of
   a square-root since all we'll be doing is comparing distances anyway
   and comparing whether one distance is greater than another is just the
   same as comparing whether one dsqd is greater than another.
*/

#include "hrect.h"

/* Returns the squared distance between x1 and x2 */
double dyv_dyv_dsqd(dyv *x1,dyv *x2);

/* Returns the squared distance between x and the closest point in hr.
   If x is in hr the result is thus zero.
*/
double hrect_dyv_min_dsqd(hrect *hr,dyv *x);

/* Returns the squared distance between x and the furthest point in hr. */
double hrect_dyv_max_dsqd(hrect *hr,dyv *x);

/* function returns the shortest possible squared distance between
   two points: one in a and the other in b. Result is thus 0 if the hrects overlap.
*/
double hrect_hrect_min_dsqd(hrect *a,hrect *b);

/* function returns the largest possible squared distance between
   two points: one in a and the other in b. 
*/
double hrect_hrect_max_dsqd(hrect *a,hrect *b);



#endif

