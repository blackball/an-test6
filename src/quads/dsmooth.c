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

#include "simplexy-common.h"

/*
 * dsmooth.c
 *
 * Smooth an image
 *
 * Mike Blanton
 * 1/2006 */

// Optimize version of dsmooth, with a separated Gaussian convolution.
int dsmooth2(float *image,
            int nx,
            int ny,
            float sigma,
            float *smooth)
{
	int i, j, npix, half, ip, jp, ist, jst, isto, jsto, ind, jnd, ioff, joff;
	float neghalfinvvar, total, scale, dx;
	float* kernel1D;

	float *smooth_temp;

	// make the kernel
	npix = 2 * ((int) ceilf(3. * sigma)) + 1;
	half = npix / 2;
	kernel1D =  malloc(npix * sizeof(float));
	neghalfinvvar = -0.5 * 1. / sigma / sigma;
	for (i = 0;i < npix;i++) {
	  dx = ((float) i - 0.5 * ((float)npix - 1.));
	  kernel1D[i] = exp((dx * dx) * neghalfinvvar);
	}

	// normalize the kernel
	total = 0.;
	for (i = 0; i < npix; i++){
	  total += kernel1D[i];
	}

	scale = 1. / total;

	for (i = 0; i < npix; i++){
	  kernel1D[i] *= scale;
	}

	smooth_temp = malloc(sizeof(float) * nx * ny);
	
	for (i = 0;i < nx*ny; i++){
	  smooth_temp[i] = 0.;
	}

	// convolve in one direction, dumping results into smooth_temp
	for (j = 0; j < ny; j++) {
	  jsto = jst = j - half;
	  jnd = j + half;
	  if (jst < 0)
	    jst = 0;
	  if (jnd > ny - 1)
	    jnd = ny - 1;
	  for (i = 0; i < nx; i++) {
	    for (jp = jst; jp <= jnd; jp++){
	      joff = jp - jsto;
	      smooth_temp[i + jp*nx] += image[i + j * nx] * kernel1D[joff];
	    }
	  }
	}

	for (i = 0;i < nx*ny; i++){
	  smooth[i] = 0.;
	}

	// convolve in the other direction, dumping results into smooth
	for (i = 0; i < nx; i++) {
	  isto = ist = i - half;
	  ind = i + half;
	  if (ist < 0)
	    ist = 0;
	  if (ind > nx - 1)
	    ind = nx - 1;
	  for (j = 0; j < ny; j++) {
	    for (ip = ist; ip <= ind; ip++){
	      ioff = ip - isto;
	      smooth[ip + j*nx] += smooth_temp[i + j * nx] * kernel1D[ioff];
	    }
	  }
	}

	FREEVEC(smooth_temp);
	FREEVEC(kernel1D);

	return (1);
} /* end dsmooth2 */


// Original version of dsmooth, non-separated kernel.
int dsmooth(float *image,
            int nx,
            int ny,
            float sigma,
            float *smooth)
{
	int i, j, npix, half, ip, jp, ist, jst, isto, jsto, ind, jnd, ioff, joff;
	float invvar, total, scale, dx, dy;
	float* kernel;

	/* make kernel */
	npix = 2 * ((int) ceilf(3. * sigma)) + 1;
	half = npix / 2;
	kernel =  malloc(npix * npix * sizeof(float));
	invvar = 1. / sigma / sigma;
	for (i = 0;i < npix;i++)
		for (j = 0;j < npix;j++) {
			dx = ((float) i - 0.5 * ((float)npix - 1.));
			dy = ((float) j - 0.5 * ((float)npix - 1.));
			kernel[i + j*npix] = exp( -0.5 * (dx * dx + dy * dy) * invvar);
		}
	total = 0.;
	for (i = 0;i < npix;i++)
		for (j = 0;j < npix;j++)
			total += kernel[i + j * npix];
	scale = 1. / total;
	for (i = 0;i < npix;i++)
		for (j = 0;j < npix;j++)
			kernel[i + j*npix] *= scale;

	for (j = 0;j < ny;j++)
		for (i = 0;i < nx;i++)
			smooth[i + j*nx] = 0.;

	for (j = 0;j < ny;j++) {
		jsto = jst = j - half;
		jnd = j + half;
		if (jst < 0)
			jst = 0;
		if (jnd > ny - 1)
			jnd = ny - 1;
		for (i = 0;i < nx;i++) {
			isto = ist = i - half;
			ind = i + half;
			if (ist < 0)
				ist = 0;
			if (ind > nx - 1)
				ind = nx - 1;
			for (jp = jst;jp <= jnd;jp++)
				for (ip = ist;ip <= ind;ip++) {
					ioff = ip - isto;
					joff = jp - jsto;
					smooth[ip + jp*nx] += image[i + j * nx] *
					                      kernel[ioff + joff * npix];
				}
		}
	}

	FREEVEC(kernel);

	return (1);
} /* end photfrac */

