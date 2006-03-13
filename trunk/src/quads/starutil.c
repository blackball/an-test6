#include <math.h>
#include <stdio.h>

#include "starutil.h"

inline double arc2distsq(double arcInRadians) {
	// inverse of distsq2arc; cosine law.
	return 2.0 * (1.0 - cos(arcInRadians));
}

inline double arcsec2distsq(double arcInArcSec) {
	return arc2distsq(arcInArcSec * M_PI / (180.0 * 60.0 * 60.0));
}

inline double distsq(double* d1, double* d2, int D) {
    double dist2;
    int i;
    dist2 = 0.0;
    for (i=0; i<D; i++) {
		dist2 += square(d1[i] - d2[i]);
    }
    return dist2;
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

inline double square(double d) {
    return d*d;
}

inline int inrange(double ra, double ralow, double rahigh) {
    if (ralow < rahigh) {
		if (ra >= ralow && ra <= rahigh)
            return 1;
        return 0;
    }

    /* handle wraparound properly */
    //if (ra <= ralow && ra >= rahigh)
    if (ra >= ralow || ra <= rahigh)
        return 1;
    return 0;
}


/* computes the 2D coordinates (x,y)  that star s would have in a
   TANGENTIAL PROJECTION defined by (centred at) star r.     */
void star_coords_2(double *s, double *r, double *x, double *y)
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

void star_midpoint_2(double* mid, double* A, double* B)
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
