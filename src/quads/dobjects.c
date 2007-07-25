/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Michael Blanton, Keir Mierle, David W. Hogg, Sam Roweis
  and Dustin Lang.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dimage.h"
#include "simplexy-common.h"

/*
 * dobjects.c
 *
 * Object detection
 *
 * Mike Blanton
 * 1/2006 */

static int *mask = NULL;

int dobjects(float *image,
             float *smooth,
             int nx,
             int ny,
             float dpsf,
             float plim,
             int *objects)
{
	int i, j, ip, jp, ist, ind, jst, jnd;
	float limit, sigma;

	dsmooth(image, nx, ny, dpsf, smooth);

	dsigma(smooth, nx, ny, (int)(10*dpsf), &sigma);
	limit = sigma * plim;

	mask = (int *) malloc(nx * ny * sizeof(int));
	for (j = 0;j < ny;j++)
		for (i = 0;i < nx;i++)
			mask[i + j*nx] = 0;
	for (j = 0;j < ny;j++) {
		jst = j - (long) (3 * dpsf);
		if (jst < 0)
			jst = 0;
		jnd = j + (long) (3 * dpsf);
		if (jnd > ny - 1)
			jnd = ny - 1;
		for (i = 0;i < nx;i++) {
			if (smooth[i + j*nx] > limit) {
				ist = i - (long) (3 * dpsf);
				if (ist < 0)
					ist = 0;
				ind = i + (long) (3 * dpsf);
				if (ind > nx - 1)
					ind = nx - 1;
				for (jp = jst;jp <= jnd;jp++)
					for (ip = ist;ip <= ind;ip++)
						mask[ip + jp*nx] = 1;
			}
		}
	}

	dfind(mask, nx, ny, objects);

	FREEVEC(mask);

	return (1);
} /* end dobjects */
