/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Michael Blanton, Keir Mierle, David W. Hogg,
  Sam Roweis and Dustin Lang.

  The Astrometry.net suite is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty
  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free
  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  02110-1301 USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "simplexy-common.h"

/*
 * dcen3x3.c
 *
 * Find center of a star inside a 3x3 image
 *
 * COMMENTS:
 *   - Convention is to make the CENTER of the first pixel (0,0).
 * BUGS:
 *
 * Mike Blanton
 * 1/2006 */

int dcen3(float f0, float f1, float f2, float *xcen)
{
	float s, d, aa, sod, kk;

	kk = (4.0/3.0);
	s = 0.5 * (f2 - f0);
	d = 2. * f1 - (f0 + f2);

	if (d <= 1.e-10*f0) {
		return (0);
	}

	aa = f1 + 0.5 * s * s / d;
	sod = s / d;
	if (!isnormal(aa) || !isnormal(s))
		return 0;
	(*xcen) = sod * (1. + kk * (0.25 * d / aa) * (1. - 4. * sod * sod)) + 1.;

	return (1);
}

int dcen3x3(float *image, float *xcen, float *ycen)
{
	float mx0, mx1, mx2;
	float my0, my1, my2;
	float bx, by, mx , my;
	int badcen = 0;

	badcen += dcen3(image[0 + 3*0], image[1 + 3*0], image[2 + 3*0], &mx0);
	badcen += dcen3(image[0 + 3*1], image[1 + 3*1], image[2 + 3*1], &mx1);
	badcen += dcen3(image[0 + 3*2], image[1 + 3*2], image[2 + 3*2], &mx2);

	badcen += dcen3(image[0 + 3*0], image[0 + 3*1], image[0 + 3*2], &my0);
	badcen += dcen3(image[1 + 3*0], image[1 + 3*1], image[1 + 3*2], &my1);
	badcen += dcen3(image[2 + 3*0], image[2 + 3*1], image[2 + 3*2], &my2);

	/* are we not okay? */
	if (badcen != 6) {
	  return (0);
	}

	/* x = (y-1) mx + bx */
	bx = (mx0 + mx1 + mx2) / 3.;
	mx = (mx2 - mx0) / 2.;

	/* y = (x-1) my + by */
	by = (my0 + my1 + my2) / 3.;
	my = (my2 - my0) / 2.;

	/* find intersection */
	(*xcen) = (mx * (by - my - 1.) + bx) / (1. + mx * my);
	(*ycen) = ((*xcen) - 1.) * my + by;

	/* check that we are in the box */
	if (((*xcen) < 0.0) || ((*xcen) > 2.0) ||
	    ((*ycen) < 0.0) || ((*ycen) > 2.0)){
	  return (0);
	}

	return (1);
} /* end dcen3x3 */
