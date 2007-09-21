/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Michael Blanton, Keir Mierle, David W. Hogg,
  Sam Roweis and Dustin Lang.

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
 * simplexy.c
 *
 * Get simple x and y values
 *
 * BUGS:
 *
 * Mike Blanton
 * 1/2006 */

// Note: To make simplexy() reentrant, do the following:
// #define SIMPLEXY_REENTRANT
// Or compile all the simplexy files with -DSIMPLEXY_REENTRANT
int simplexy(float *image,
             int nx,
             int ny,
             float dpsf,    /* gaussian psf width; 1 is usually fine */
             float plim,    /* significance to keep; 8 is usually fine */
             float dlim,    /* closest two peaks can be; 1 is usually fine */
             float saddle,  /* saddle difference (in sig); 3 is usually fine */
             int maxper,    /* maximum number of peaks per object; 1000 */
             int maxnpeaks, /* maximum number of peaks total; 100000 */
             int maxsize,   /* maximum size for extended objects: 150 */
             int halfbox,    /* size for sliding sky estimation box */
             float *sigma,
             float *x,
             float *y,
             float *flux,
             int *npeaks,
             int verbose) {
	float minpeak;
	int i, j;

	float *invvar = NULL;
	float *simage = NULL;
	int *oimage = NULL;
	float *smooth = NULL;

    if (verbose) {
        fprintf(stderr, "simplexy: nx=%d, ny=%d\n", nx, ny);
        fprintf(stderr, "simplexy: dpsf=%f, plim=%f, dlim=%f, saddle=%f\n",
                dpsf, plim, dlim, saddle);
        fprintf(stderr, "simplexy: maxper=%d, maxnpeaks=%d, maxsize=%d, halfbox=%d\n",
                maxper, maxnpeaks, maxsize, halfbox);
    }

	/* determine an estimate of the noise in the image (sigma) assuming the
	 * noise is iid gaussian, by sampling at regular intervals, and comparing
	 * the difference between pixels separated by a 5-pixel diagonal gap. */
	dsigma(image, nx, ny, 5, sigma);
	invvar = (float *) malloc(nx * ny * sizeof(float));
    for (j = 0;j < ny;j++)
		for (i = 0;i < nx;i++)
			invvar[i + j*nx] = 1. / ((*sigma) * (*sigma));
	minpeak = (*sigma);
    if (verbose)
        fprintf(stderr, "simplexy: dsigma() found sigma=%g.\n", *sigma);

	/* median smooth */
	/* NB: over-write simage to save malloc */
	simage = (float *) malloc(nx * ny * sizeof(float));
	dmedsmooth(image, invvar, nx, ny, halfbox, simage);
	for (j = 0;j < ny;j++)
		for (i = 0;i < nx;i++)
			simage[i + j*nx] = image[i + j*nx] - simage[i + j*nx];
    if (verbose)
        fprintf(stderr, "simplexy: finished dmedsmooth().\n");
	FREEVEC(invvar);

	/* find objects */
	smooth = (float *) malloc(nx * ny * sizeof(float));
	oimage = (int *) malloc(nx * ny * sizeof(int));
	dobjects(simage, smooth, nx, ny, dpsf, plim, oimage);
    if (verbose)
        fprintf(stderr, "simplexy: finished dobjects().\n");
	FREEVEC(smooth);

	/* find all peaks within each object */
	dallpeaks(simage, nx, ny, oimage, x, y, npeaks, dpsf,
	          (*sigma), dlim, saddle, maxper, maxnpeaks, minpeak, maxsize);
    if (verbose)
        fprintf(stderr, "simplexy: dallpeaks() found %i peaks.\n", *npeaks);
	FREEVEC(oimage);

	for (i = 0;i < (*npeaks);i++)
		flux[i] = simage[((int) (x[i] + 0.5)) + ((int) (y[i] + 0.5)) * nx];
	FREEVEC(simage);

	return (1);
}
