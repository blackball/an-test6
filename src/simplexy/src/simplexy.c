/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Michael Blanton, Keir Mierle, David W. Hogg,
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
#include "common.h"

/*
 * simplexy.c
 *
 * Get simple x and y values
 *
 * BUGS:
 *
 * Mike Blanton
 * 1/2006 */

static float *invvar = NULL;
static float *mimage = NULL;
static float *simage = NULL;
static int *oimage = NULL;
static float *smooth = NULL;

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
	     int skybox,    /* size for sliding sky estimation box */
             float *sigma,
             float *x,
             float *y,
             float *flux,
             int *npeaks)
{
	float minpeak;
	int i, j;

    fprintf(stderr, "simplexy: nx=%d, ny=%d\n",nx,ny);
    fprintf(stderr, "simplexy: dpsf=%f, plim=%f, dlim=%f, saddle=%f\n",
	    dpsf,plim,dlim,saddle);
    fprintf(stderr, "simplexy: maxper=%d, maxnpeaks=%d, maxsize=%d, skybox=%d\n",
	    maxper,maxnpeaks,maxsize,skybox);

	/* determine sigma */
	dsigma(image, nx, ny, 5, sigma);
	invvar = (float *) malloc(nx * ny * sizeof(float));
	for (j = 0;j < ny;j++)
		for (i = 0;i < nx;i++)
			invvar[i + j*nx] = 1. / ((*sigma) * (*sigma));
	minpeak = (*sigma);

	/* median smooth */
	mimage = (float *) malloc(nx * ny * sizeof(float));
	simage = (float *) malloc(nx * ny * sizeof(float));
	dmedsmooth(image, invvar, nx, ny, skybox, mimage);
	for (j = 0;j < ny;j++)
		for (i = 0;i < nx;i++)
			simage[i + j*nx] = image[i + j * nx] - mimage[i + j * nx];

	/* find objects */
	smooth = (float *) malloc(nx * ny * sizeof(float));
	oimage = (int *) malloc(nx * ny * sizeof(int));
	dobjects(simage, smooth, nx, ny, dpsf, plim, oimage);

	/* find all peaks within each object */
	dallpeaks(simage, nx, ny, oimage, x, y, npeaks, (*sigma), dlim, saddle,
	          maxper, maxnpeaks, minpeak, maxsize);

	for (i = 0;i < (*npeaks);i++)
		flux[i] = simage[((int) (x[i]+0.5)) + ((int) (y[i]+0.5)) * nx];

	FREEVEC(invvar);
	FREEVEC(mimage);
	FREEVEC(simage);
	FREEVEC(oimage);
	FREEVEC(smooth);

	return (1);
} /* end dmedsmooth */
