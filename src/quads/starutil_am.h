#ifndef STARUTIL_AM_H
#define STARUTIL_AM_H

#include "starutil.h"
#include "KD/ambs.h"
#include "KD/amdyv_array.h"

#define DEFAULT_KDRMIN 50
#define DEFAULT_IDXSCALE 5.0
#define DEFAULT_OVERSAMPLE 5

typedef ivec quad;
typedef dyv code;
typedef dyv star;

/*
  typedef dyv xy;

  #define mk_xy(n) (xy *)mk_dyv(DIM_XY*n)
  #define free_xy(s) free_dyv((dyv *)s)
  #define xy_ref(s,i) dyv_ref((dyv *)s,i)
  #define xy_refx(s,i) dyv_ref((dyv *)s,DIM_XY*i)
  #define xy_refy(s,i) dyv_ref((dyv *)s,DIM_XY*i+1)
  #define xy_size(s) (dyv_size(s)/2)
  #define xy_set(s,i,v) dyv_set((dyv *)s,i,v)
  #define xy_setx(s,i,v) dyv_set((dyv *)s,DIM_XY*i,v)
  #define xy_sety(s,i,v) dyv_set((dyv *)s,DIM_XY*i+1,v)

  typedef dyv_array xyarray;
  #define xya_ref(a,i) dyv_array_ref(a,i)
  #define xya_set(a,i,v) dyv_array_set(a,i,v)
  #define mk_xyarray(n) (xyarray *)mk_dyv_array(n)
  #define free_xyarray(s) free_dyv_array((dyv_array *)s)
*/

typedef ivec_array quadarray;
typedef dyv_array codearray;
typedef dyv_array stararray;

#define mk_star() (star *)mk_dyv(DIM_STARS)
#define free_star(s) free_dyv((dyv *)s)
#define star_ref(s,i) dyv_ref((dyv *)s,i)
#define star_set(s,i,v) dyv_set((dyv *)s,i,v)

#define mk_stard(d) (star *)mk_dyv(d)
#define mk_code() (code *)mk_dyv(DIM_CODES)
#define mk_coded(d) (code *)mk_dyv(d)
#define mk_quad() (quad *)mk_ivec(DIM_CODES)
#define mk_quadd(d) (quad *)mk_ivec(d)
#define free_code(c) free_dyv((dyv *)c)
#define free_quad(q) free_ivec((ivec *)q)

#define code_ref(c,i) dyv_ref((dyv *)c,i)
#define quad_ref(q,i) ivec_ref((ivec *)q,i)

#define code_set(c,i,v) dyv_set((dyv *)c,i,v)
#define quad_set(q,i,j) ivec_set((ivec *)q,i,j)

#define star_array_ref(s, i) (star*)dyv_array_ref(s, i)
#define star_array_set(s, i, v) (star*)dyv_array_set(s, i, v)

#define mk_stararray(n) (stararray *)mk_dyv_array(n)
#define mk_quadarray(n) (quadarray *)mk_ivec_array(n)
#define mk_codearray(n) (codearray *)mk_dyv_array(n)
#define free_stararray(s) free_dyv_array((dyv_array *)s)
#define free_quadarray(q) free_ivec_array((ivec_array *)q)
#define free_codearray(c) free_dyv_array((dyv_array *)c)

void star_coords_old(star *s, star *r, double *x, double *y);
void star_midpoint_old(star *M, star *A, star *B);

#endif
