#include <math.h>
#include <stdio.h>
#include <assert.h>

#include "starutil.h"
#include "mathutil.h"

inline void radec2xyz(double ra, double dec,
					  double* x, double* y, double* z) {
	*x = cos(dec) * cos(ra);
	*y = cos(dec) * sin(ra);
	*z = sin(dec);
}

inline void radec2xyzarr(double ra, double dec, double* xyz) {
	xyz[0] = cos(dec) * cos(ra);
	xyz[1] = cos(dec) * sin(ra);
	xyz[2] = sin(dec);
}

inline void project_equal_area(double x, double y, double z, double* projx, double* projy) {
	double Xp = x*sqrt(1./(1. + z));
	double Yp = y*sqrt(1./(1. + z));
	Xp = 0.5 * (1.0 + Xp);
	Yp = 0.5 * (1.0 + Yp);
	assert(Xp >= 0.0 && Xp <= 1.0);
	assert(Yp >= 0.0 && Yp <= 1.0);
	*projx = Xp;
	*projy = Yp;
}

inline void project_hammer_aitoff_x(double x, double y, double z, double* projx, double* projy) {
	double theta = atan(x/z);
	double r = sqrt(x*x+z*z);
	double zp, xp;
	/* Hammer-Aitoff projection with x-z plane compressed to purely +z side
	 * of xz plane */
	if (z < 0) {
		if (x < 0) {
			theta = theta - M_PI;
		} else {
			theta = M_PI + theta;
		}
	}
	theta /= 2.0;
	zp = r*cos(theta);
	xp = r*sin(theta);
	assert(zp >= -0.01);
	project_equal_area(xp, y, zp, projx, projy);
}

/* makes a star object located uniformly at random within the limits given
   on the sphere */
void make_rand_star(double* star, double ramin, double ramax,
					double decmin, double decmax)
{
	double decval, raval;
	if (ramin < 0.0)
		ramin = 0.0;
	if (ramax > (2*PIl))
		ramax = 2 * PIl;
	if (decmin < -PIl / 2.0)
		decmin = -PIl / 2.0;
	if (decmax > PIl / 2.0)
		decmax = PIl / 2.0;

	decval = asin(uniform_sample(sin(decmin), sin(decmax)));
	raval = uniform_sample(ramin, ramax);
	star[0] = radec2x(raval, decval);
	star[1] = radec2y(raval, decval);
	star[2] = radec2z(raval, decval);
}

inline double arc2distsq(double arcInRadians) {
	// inverse of distsq2arc; cosine law.
	return 2.0 * (1.0 - cos(arcInRadians));
}

inline double arcsec2distsq(double arcInArcSec) {
	return arc2distsq(arcInArcSec * M_PI / (180.0 * 60.0 * 60.0));
}

inline double distsq2arc(double dist2) {
	// cosine law: c^2 = a^2 + b^2 - 2 a b cos C
	// c^2 is dist2.  We want C.
	// a = b = 1
	// c^2 = 1 + 1 - 2 cos C
	// dist2 = 2( 1 - cos C )
	// 1 - (dist2 / 2) = cos C
	// C = acos(1 - dist2 / 2)
	return acos(1.0 - dist2 / 2.0);
}

/* computes the 2D coordinates (x,y)  that star s would have in a
   TANGENTIAL PROJECTION defined by (centred at) star r.     */
void star_coords(double *s, double *r, double *x, double *y)
{
	double sdotr = s[0] * r[0] + s[1] * r[1] + s[2] * r[2];
	if (sdotr <= 0.0) {
		fprintf(stderr, "ERROR (star_coords) -- s dot r <=0; undefined projection.\n");
		return ;
	}

	if (r[2] == 1.0) {
		*x = s[0] / s[2];
		*y = s[1] / s[2];
	} else if (r[2] == -1.0) {
		*x = s[0] / s[2];
		*y = -s[1] / s[2];
	} else {
		double etax, etay, etaz, xix, xiy, xiz, eta_norm;
		// eta is a vector perpendicular to r
		etax = -r[1];
		etay = + r[0];
		etaz = 0.0;
		eta_norm = sqrt(etax * etax + etay * etay);
		etax /= eta_norm;
		etay /= eta_norm;
		// xi =  r cross eta
		xix = -r[2] * etay;
		xiy = r[2] * etax;
		xiz = r[0] * etay - r[1] * etax;

		*x = s[0] * xix / sdotr +
		     s[1] * xiy / sdotr +
		     s[2] * xiz / sdotr;
		*y = s[0] * etax / sdotr +
		     s[1] * etay / sdotr;
	}
}

void star_midpoint(double* mid, double* A, double* B)
{
	double len;
	// we don't actually need to divide by 2 because
	// we immediately renormalize it...
	mid[0] = A[0] + B[0];
	mid[1] = A[1] + B[1];
	mid[2] = A[2] + B[2];
	len = sqrt(square(mid[0]) + square(mid[1]) + square(mid[2]));
	mid[0] /= len;
	mid[1] /= len;
	mid[2] /= len;
}
