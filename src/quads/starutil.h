#ifndef starutil_H
#define starutil_H

#include "KD/ambs.h"
#include "KD/amdyv_array.h"

#define DIM_STARS 3
#define DIM_CODES 4
#define DIM_QUADS 4
#define DIM_XY 2

#define DEFAULT_KDRMIN 50
#define DEFAULT_IDXSCALE 5.0
#define DEFAULT_OVERSAMPLE 5

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define PIl          3.1415926535897932384626433832795029L  /* pi */

#define SIDX_MAX ULONG_MAX
#define QIDX_MAX ULONG_MAX

#define DEFAULT_RAMIN 0.0
#define DEFAULT_RAMAX (2.0*PIl)
#define DEFAULT_DECMIN (-PIl/2.0)
#define DEFAULT_DECMAX (+PIl/2.0)


#define ABCD_ORDER 0
#define BACD_ORDER 1
#define ABDC_ORDER 2
#define BADC_ORDER 3

typedef unsigned long int qidx;
typedef unsigned long int sidx;
typedef unsigned short int dimension;

typedef ivec quad;
typedef dyv code;
typedef dyv star;
typedef dyv xy;

typedef ivec_array quadarray;
typedef dyv_array codearray;
typedef dyv_array stararray;
typedef dyv_array xyarray;

inline double square(double d);

#define mk_star() (star *)mk_dyv(DIM_STARS)
#define mk_stard(d) (star *)mk_dyv(d)
#define mk_code() (code *)mk_dyv(DIM_CODES)
#define mk_coded(d) (code *)mk_dyv(d)
#define mk_quad() (quad *)mk_ivec(DIM_CODES)
#define mk_quadd(d) (quad *)mk_ivec(d)
#define mk_xy(n) (xy *)mk_dyv(DIM_XY*n)
#define free_star(s) free_dyv((dyv *)s)
#define free_code(c) free_dyv((dyv *)c)
#define free_quad(q) free_ivec((ivec *)q)
#define free_xy(s) free_dyv((dyv *)s)

#define star_ref(s,i) dyv_ref((dyv *)s,i)
#define code_ref(c,i) dyv_ref((dyv *)c,i)
#define quad_ref(q,i) ivec_ref((ivec *)q,i)
#define xya_ref(a,i) dyv_array_ref(a,i)
#define xy_ref(s,i) dyv_ref((dyv *)s,i)
#define xy_refx(s,i) dyv_ref((dyv *)s,DIM_XY*i)
#define xy_refy(s,i) dyv_ref((dyv *)s,DIM_XY*i+1)
#define xy_size(s) (dyv_size(s)/2)

#define star_set(s,i,v) dyv_set((dyv *)s,i,v)
#define code_set(c,i,v) dyv_set((dyv *)c,i,v)
#define quad_set(q,i,j) ivec_set((ivec *)q,i,j)
#define xy_set(s,i,v) dyv_set((dyv *)s,i,v)
#define xy_setx(s,i,v) dyv_set((dyv *)s,DIM_XY*i,v)
#define xy_sety(s,i,v) dyv_set((dyv *)s,DIM_XY*i+1,v)

#define mk_stararray(n) (stararray *)mk_dyv_array(n)
#define mk_quadarray(n) (quadarray *)mk_ivec_array(n)
#define mk_codearray(n) (codearray *)mk_dyv_array(n)
#define mk_xyarray(n) (xyarray *)mk_dyv_array(n)
#define free_stararray(s) free_dyv_array((dyv_array *)s)
#define free_quadarray(q) free_ivec_array((ivec_array *)q)
#define free_codearray(c) free_dyv_array((dyv_array *)c)
#define free_xyarray(s) free_dyv_array((dyv_array *)s)

#define rad2deg(r) (180.0*r/(double)PIl)
#define deg2rad(d) (d*(double)PIl/180.0)
#define rad2arcmin(r) (10800.0*r/(double)PIl)
#define arcmin2rad(a) (a*(double)PIl/10800.0)
#define rad2arcsec(r) (648000.0*r/(double)PIl)
#define arcsec2rad(a) (a*(double)PIl/648000.0)
#define radec2x(r,d) (cos(d)*cos(r)) // r,d in radians
#define radec2y(r,d) (cos(d)*sin(r)) // r,d in radians
#define radec2z(r,d) (sin(d))        // d in radians
#define xy2ra(x,y) ((atan2(y,x)>=0.0)?(atan2(y,x)):(2*(double)PIl+atan2(y,x))) // result in radians
#define z2dec(z) (asin(z)) // result in radians

#define radscale2xyzscale(r) (sqrt(2.0-2.0*cos(r/2.0)))

star *make_rand_star(double ramin, double ramax,
                     double decmin, double decmax);
void star_coords(star *s, star *r, double *x, double *y);
void star_midpoint(star *M, star *A, star *B);

#define HELP_ERR -101
#define OPT_ERR -201

#endif
