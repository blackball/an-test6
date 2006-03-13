#ifndef STARUTIL_H
#define STARUTIL_H

#include <unistd.h>
#include "bl.h"

#define DIM_STARS 3
#define DIM_CODES 4
#define DIM_QUADS 4
#define DIM_XY 2

typedef char bool;
/*
  bool TRUE = 1;
  bool FALSE = 0;
*/

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

typedef unsigned int uint;

inline double square(double d);

inline int inrange(double ra, double ralow, double rahigh);

inline double distsq(double* d1, double* d2, int D);

#define rad2deg(r) (180.0*(r)/(double)PIl)
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

// Converts a distance-squared between two points on the
// surface of the unit sphere into the angle between the
// rays from the center of the sphere to the points, in
// radians.
inline double distsq2arc(double dist2);

inline double arc2distsq(double arcInRadians);

inline double arcsec2distsq(double arcInArcSec);

#define radscale2xyzscale(r) (sqrt(2.0-2.0*cos(r/2.0)))

#define HELP_ERR -101
#define OPT_ERR -201

typedef dl xy;
#define mk_xy(n) dl_new((n)*2)
#define free_xy(x) dl_free(x)
#define xy_ref(x, i) dl_get(x, i)
#define xy_refx(x, i) dl_get(x, 2*(i))
#define xy_refy(x, i) dl_get(x, (2*(i))+1)
#define xy_size(x) (dl_size(x)/2)
#define xy_set(x,i,v) dl_set(x,i,v)
#define xy_setx(x,i,v) dl_set(x,2*(i),v)
#define xy_sety(x,i,v) dl_set(x,2*(i)+1,v)

typedef pl xyarray;
#define mk_xyarray(n) pl_new(n)
#define free_xyarray(l) pl_free(l)
#define xya_ref(l, i) (xy*)pl_get((l), (i))
#define xya_set(l, i, v) pl_set((l), (i), (v))
#define xya_size(l) pl_size(l)
#endif
