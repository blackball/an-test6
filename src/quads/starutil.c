#include "starutil.h"

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

/* makes a star object located uniformly at random within the limits given
   on the sphere */
star *make_rand_star(double ramin, double ramax,
                     double decmin, double decmax)
{
	double decval, raval;
	star *thestar = mk_star();
	if (ramin < 0.0)
		ramin = 0.0;
	if (ramax > (2*PIl))
		ramax = 2 * PIl;
	if (decmin < -PIl / 2.0)
		decmin = -PIl / 2.0;
	if (decmax > PIl / 2.0)
		decmax = PIl / 2.0;
	if (thestar != NULL) {
		decval = asin(range_random(sin(decmin), sin(decmax)));
		raval = range_random(ramin, ramax);
		star_set(thestar, 0, cos(decval)*cos(raval));
		star_set(thestar, 1, cos(decval)*sin(raval));
		star_set(thestar, 2, sin(decval));
	}
	return thestar;
}



/* computes the 2D coordinates (x,y)  that star s would have in a
   TANGENTIAL PROJECTION defined by (centred at) star r.     */

void star_coords(star *s, star *r, double *x, double *y)
{
#ifndef AMFAST
	if (s == NULL) {
		fprintf(stderr, "ERROR (star_coords) -- s NULL\n");
		return ;
	}
	if (r == NULL) {
		fprintf(stderr, "ERROR (star_coords) -- r NULL\n");
		return ;
	}
#endif

#if FALSE
	double chklen;
	chklen = star_ref(s, 0) * star_ref(s, 0) + star_ref(s, 1) * star_ref(s, 1) +
	         star_ref(s, 2) * star_ref(s, 2);
	if (fabs(chklen - 1.0) > 0.0001)
		fprintf(stderr, "ERROR (star_coords) -- s has length %f\n", chklen);
	chklen = star_ref(r, 0) * star_ref(r, 0) + star_ref(r, 1) * star_ref(r, 1) +
	         star_ref(r, 2) * star_ref(r, 2);
	if (fabs(chklen - 1.0) > 0.0001)
		fprintf(stderr, "ERROR (star_coords) -- r has length %f\n", chklen);
#endif

	double sdotr = star_ref(s, 0) * star_ref(r, 0) +
	               star_ref(s, 1) * star_ref(r, 1) +
	               star_ref(s, 2) * star_ref(r, 2);
	if (sdotr <= 0.0) {
		fprintf(stderr, "ERROR (star_coords) -- s dot r <=0; undefined projection.\n");
		return ;
	}

	if (star_ref(r, 2) == 1.0) {
		*x = star_ref(s, 0) / star_ref(s, 2);
		*y = star_ref(s, 1) / star_ref(s, 2);
	} else if (star_ref(r, 2) == -1.0) { //???
		*x = star_ref(s, 0) / star_ref(s, 2);
		*y = -star_ref(s, 1) / star_ref(s, 2);
	} else {
		double etax, etay, etaz, xix, xiy, xiz, eta_norm;

		// eta is a vector perpendicular to r
		etax = -star_ref(r, 1);
		etay = + star_ref(r, 0);
		etaz = 0.0;
		eta_norm = sqrt(etax * etax + etay * etay);
		etax /= eta_norm;
		etay /= eta_norm;

		// xi =  r cross eta

		//xix = star_ref(r,1)*etaz-star_ref(r,2)*etay;
		xix = -star_ref(r, 2) * etay;
		//xiy = star_ref(r,2)*etax-star_ref(r,0)*etaz;
		xiy = star_ref(r, 2) * etax;
		xiz = star_ref(r, 0) * etay - star_ref(r, 1) * etax;

		*x = star_ref(s, 0) * xix / sdotr +
		     star_ref(s, 1) * xiy / sdotr +
		     star_ref(s, 2) * xiz / sdotr;
		*y = star_ref(s, 0) * etax / sdotr +
		     star_ref(s, 1) * etay / sdotr;
		//+star_ref(s,2)*etaz/sdotr;
	}

	return ;
}


/* sets the coordinates of star M to be the midpoint of the coordinates
   of stars A,B. DOES NOT allocate a new star object for M.
   does this by averaging and then projecting
   back onto the surface of the sphere                            */
void star_midpoint(star *M, star *A, star *B)
{
	double len;
	star_set(M, 0, (star_ref(A, 0) + star_ref(B, 0)) / 2);
	star_set(M, 1, (star_ref(A, 1) + star_ref(B, 1)) / 2);
	star_set(M, 2, (star_ref(A, 2) + star_ref(B, 2)) / 2);
	len = sqrt(star_ref(M, 0) * star_ref(M, 0) + star_ref(M, 1) * star_ref(M, 1) +
	           star_ref(M, 2) * star_ref(M, 2));
	star_set(M, 0, star_ref(M, 0) / len);
	star_set(M, 1, star_ref(M, 1) / len);
	star_set(M, 2, star_ref(M, 2) / len);
	return ;
}

