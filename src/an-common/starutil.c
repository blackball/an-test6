/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <math.h>
#include <stdio.h>
#include <assert.h>

#include "starutil.h"
#include "mathutil.h"
#include "keywords.h"

#define POGSON 2.51188643150958
#define LOGP   0.92103403719762

double mag2flux(double mag) {
	return pow(POGSON, -mag);
}

double flux2mag(double flux) {
	return -log(flux) * LOGP;
}

inline void xyz2radec(double x, double y, double z, double *ra, double *dec) {
	*ra = xy2ra(x, y);
	*dec = z2dec(z);
}

inline void xyzarr2radec(double* xyz, double *ra, double *dec) {
	xyz2radec(xyz[0], xyz[1], xyz[2], ra, dec);
}

inline void radec2xyz(double ra, double dec,
					  double* x, double* y, double* z) {
	double cosdec = cos(dec);
	*x = cosdec * cos(ra);
	*y = cosdec * sin(ra);
	*z = sin(dec);
}

inline void radec2xyzarr(double ra, double dec, double* xyz) {
	xyz[0] = cos(dec) * cos(ra);
	xyz[1] = cos(dec) * sin(ra);
	xyz[2] = sin(dec);
}
inline void radecdeg2xyzarr(double ra, double dec, double* xyz) {
	radec2xyzarr(deg2rad(ra),deg2rad(dec), xyz);
}

// xyz stored as xyzxyzxyz.
inline void radec2xyzarrmany(double *ra, double *dec, double* xyz, int n) {
	int i;
	for (i=0; i<n; i++) {
		radec2xyzarr(ra[i], dec[i], xyz+3*i);
	}
}

inline void radecdeg2xyzarrmany(double *ra, double *dec, double* xyz, int n) {
	int i;
	for (i=0; i<n; i++) {
		radec2xyzarr(deg2rad(ra[i]), deg2rad(dec[i]), xyz+3*i);
	}
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

Const inline double arc2distsq(double arcInRadians) {
	// inverse of distsq2arc; cosine law.
	return 2.0 * (1.0 - cos(arcInRadians));
}

Const inline double arcsec2distsq(double arcInArcSec) {
   return arc2distsq(arcsec2rad(arcInArcSec));
}

Const inline double distsq2arcsec(double dist2) {
	return rad2arcsec(distsq2arc(dist2));
}

Const inline double distsq2arc(double dist2) {
	// cosine law: c^2 = a^2 + b^2 - 2 a b cos C
	// c^2 is dist2.  We want C.
	// a = b = 1
	// c^2 = 1 + 1 - 2 cos C
	// dist2 = 2( 1 - cos C )
	// 1 - (dist2 / 2) = cos C
	// C = acos(1 - dist2 / 2)
	return acos(1.0 - dist2 / 2.0);
}




/* computes the 2D coordinates (x,y) (in units of a celestial sphere of radius 1)
   that star s would have in a TANGENTIAL PROJECTION defined by (centred at) star r.
   s and r are both given in xyz coordinates, the parameters are pointers to arrays of size3
	WARNING -- this code assumes s and r are UNIT vectors (ie normalized to have length 1)
	the resulting x direction is increasing DEC, the resulting y direction is increasing RA
	which might not be the normal convention
*/
inline void star_coords(double *s, double *r, double *x, double *y)
{
	double sdotr = s[0] * r[0] + s[1] * r[1] + s[2] * r[2];
	assert(sdotr > 0.0);
	if (unlikely(r[2] == 1.0)) {
		double inv_s2 = 1.0 / s[2];
		*x = s[0] * inv_s2;
		*y = s[1] * inv_s2;
	} else if (unlikely(r[2] == -1.0)) {
		double inv_s2 = 1.0 / s[2];
		*x =  s[0] * inv_s2;
		*y = -s[1] * inv_s2;
	} else {
		double etax, etay, etaz, xix, xiy, xiz, eta_norm;
		double inv_en, inv_sdotr;
		// eta is a vector perpendicular to r pointing in the direction of increasing RA
 		etax = -r[1];
		etay =  r[0];
		etaz = 0.0;
		eta_norm = hypot(etax, etay); //sqrt(etax * etax + etay * etay);
		inv_en = 1.0 / eta_norm;
		etax *= inv_en;
		etay *= inv_en;
		// xi =  r cross eta, a vector pointing northwards, in direction of increasing DEC
		xix = -r[2] * etay;
		xiy =  r[2] * etax;
		xiz =  r[0] * etay - r[1] * etax;
		inv_sdotr = 1.0 / sdotr;
		*x = (s[0] * xix + s[1] * xiy + s[2] * xiz) * inv_sdotr;
		*y = (s[0] * etax + s[1] * etay) * inv_sdotr;
	}
}

inline void star_midpoint(double* mid, double* A, double* B) {
	double len;
	double invlen;
	// we don't divide by 2 because we immediately renormalize it...
	mid[0] = A[0] + B[0];
	mid[1] = A[1] + B[1];
	mid[2] = A[2] + B[2];
	//len = sqrt(square(mid[0]) + square(mid[1]) + square(mid[2]));
	len = sqrt(mid[0] * mid[0] + mid[1] * mid[1] + mid[2] * mid[2]);
	invlen = 1.0 / len;
	mid[0] *= invlen;
	mid[1] *= invlen;
	mid[2] *= invlen;
}

