#include "starutil_am.h"

/* computes the 2D coordinates (x,y)  that star s would have in a
   TANGENTIAL PROJECTION defined by (centred at) star r.     */

void star_coords_old(star *s, star *r, double *x, double *y)
{
	double sdotr;
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

	sdotr = star_ref(s, 0) * star_ref(r, 0) +
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
void star_midpoint_old(star *M, star *A, star *B)
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

