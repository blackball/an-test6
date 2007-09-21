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
#include <sys/param.h>

#include "simplexy-common.h"

/*
 * dfind.c
 *
 * Find non-zero objects in a binary image.
 *
 * Mike Blanton
 * 1/2006
 *
 * dfind2() Keir Mierle 2007
 */

#define MAX_GROUPS 10000

/*
 * This code does connected component analysis, but instead of returning a list
 * of components, it does the following crazy thing: it returns an image where
 * each component is all numbered the same. For example, if your input is:
 *
 *  . . . . . . . . .
 *  . . . 1 . . . . .
 *  . . 1 1 1 . . . .
 *  . . . 1 . . . . .
 *  . . . . . . . . .
 *  . 1 1 . . . 1 . .
 *  . 1 1 . . 1 1 1 .
 *  . 1 1 . . . 1 . .
 *  . . . . . . . . .
 *
 * where . is 0. Then your output is:
 *
 *  . . . . . . . . .
 *  . . . 0 . . . . .
 *  . . 0 0 0 . . . .
 *  . . . 0 . . . . .
 *  . . . . . . . . .
 *  . 1 1 . . . 2 . .
 *  . 1 1 . . 2 2 2 .
 *  . 1 1 . . . 2 . .
 *  . . . . . . . . .
 *
 * where . is now -1. Diagonals are considered connections, so the following is
 * a single component:
 *  . . . . .
 *  . 1 . 1 .
 *  . . 1 . .
 *  . 1 . 1 .
 *  . . . . .
 */

int dfind2(int *image,
          int nx,
          int ny,
          int *object)
{
	int ix, iy, i, j;
	short int *equivs = malloc(sizeof(short int)*MAX_GROUPS);
	short int *number = malloc(sizeof(short int)*MAX_GROUPS);
	short int maxlabel=0;
	short int maxcontiguouslabel=0;

	/* Find blobs and track equivalences */
	for (iy=0; iy<ny; iy++) {
		for (ix=0; ix<nx; ix++) {
		  int thislabel,thislabelmin;

			object[nx*iy+ix] = -1;
			if (!image[nx*iy+ix])
				continue;

			if (ix && image[nx*iy+ix-1]) {
				/* Old group */
				object[nx*iy+ix] = object[nx*iy+ix-1];

			} else {
				/* New blob */
				object[nx*iy+ix] = maxlabel++;
				equivs[object[nx*iy+ix]] = object[nx*iy+ix];
			}

			thislabel  = object[nx*iy + ix];

			/* Compute minimum equivalence label for this pixel */
			thislabelmin = thislabel;
			while (equivs[thislabelmin] != thislabelmin)
				thislabelmin = equivs[thislabelmin];

			if (iy) {
				for (i=MAX(0,ix-1); i<MIN(ix+2,nx); i++) {
					if (image[nx*(iy-1)+i]) {
						int otherlabel = object[nx*(iy-1) + i];

						/* Find min of the other */
						int otherlabelmin = otherlabel;
						while (equivs[otherlabelmin] != otherlabelmin)
							otherlabelmin = equivs[otherlabelmin];

						/* Merge groups if necessary */
						if (thislabelmin != otherlabelmin) {
							int oldlabelmin = MAX(thislabelmin, otherlabelmin);
							int newlabelmin = MIN(thislabelmin, otherlabelmin);
							printf("RELABEL:\n");
							for (j=0; j<maxlabel; j++) 
								if (equivs[j] == oldlabelmin) {
									printf("ix%d iy%d making equivs[%d] = %d, was %d\n", ix, iy, j, newlabelmin, equivs[j]);
									equivs[j] = newlabelmin;
								}
							thislabelmin = newlabelmin;
							equivs[object[nx*iy+ix]] = newlabelmin;
						}
					} 
				}
			}
		}
	}

	for (i=0; i<maxlabel; i++) {
		number[i] = -1;
	}
	
	/* debug print */
	for (iy=0; iy<ny; iy++) {
		for (ix=0; ix<nx; ix++) {
			int n = object[nx*iy+ix];
			if (n == -1)
				printf("  ");
			else
				printf(" %d", n);
		}
		printf("\n");
	}
		printf("... \n");

	/* Re-label the groups */
	for (iy=0; iy<ny; iy++) {
		for (ix=0; ix<nx; ix++) {
			int origlabel = object[nx*iy+ix];
			if (origlabel != -1) {
				int minlabel = equivs[origlabel];
				if (number[minlabel] == -1) {
					number[minlabel] = maxcontiguouslabel++;
				}
				object[nx*iy+ix] = number[minlabel];
			}
		}
	}

	/* debug print */
	for (iy=0; iy<ny; iy++) {
		for (ix=0; ix<nx; ix++) {
			int n = object[nx*iy+ix];
			if (n == -1)
				printf(" . ");
			else
				printf("%3d", n);
		}
		printf("\n");
	}

	free(equivs);
	free(number);
	return 1;
}

int dfind(int *image,
          int nx,
          int ny,
          int *object)
{
	int i, ip, j, jp, k, kp, l, ist, ind, jst, jnd, igroup, minearly, checkearly, tmpearly;
	int ngroups;

	int* mapgroup = (int *) malloc((size_t) nx * ny * sizeof(int));
	int* matches = (int *) malloc((size_t) nx * ny * 9 * sizeof(int));
	int* nmatches = (int *) malloc((size_t) nx * ny * sizeof(int));

        if (!mapgroup || !matches || !nmatches) {
            fprintf(stderr, "Failed to allocate memory in dfind.c\n");
            exit(-1);
        }

	for (k = 0;k < nx*ny;k++)
		object[k] = -1;
	for (k = 0;k < nx*ny;k++)
		mapgroup[k] = -1;
	for (k = 0;k < nx*ny;k++)
		nmatches[k] = 0;
	for (k = 0;k < nx*ny*9;k++)
		matches[k] = -1;

	/* find matches */
	for (j = 0;j < ny;j++) {
		jst = j - 1;
		jnd = j + 1;
		if (jst < 0)
			jst = 0;
		if (jnd > ny - 1)
			jnd = ny - 1;
		for (i = 0;i < nx;i++) {
			ist = i - 1;
			ind = i + 1;
			if (ist < 0)
				ist = 0;
			if (ind > nx - 1)
				ind = nx - 1;
			k = i + j * nx;
			if (image[k]) {
				for (jp = jst;jp <= jnd;jp++)
					for (ip = ist;ip <= ind;ip++) {
						kp = ip + jp * nx;
						if (image[kp]) {
							matches[9*k + nmatches[k]] = kp;
							nmatches[k]++;
						}
					}
			} /* end if */
		}
	}

	/* group pixels on matches */
	igroup = 0;
	for (k = 0;k < nx*ny;k++) {
		if (image[k]) {
			minearly = igroup;
			for (l = 0;l < nmatches[k];l++) {
				kp = matches[9 * k + l];
				checkearly = object[kp];
				if (checkearly >= 0) {
					while (mapgroup[checkearly] != checkearly) {
						checkearly = mapgroup[checkearly];
					}
					if (checkearly < minearly)
						minearly = checkearly;
				}
			}

			if (minearly == igroup) {
				mapgroup[igroup] = igroup;
				for (l = 0;l < nmatches[k];l++) {
					kp = matches[9 * k + l];
					object[kp] = igroup;
				}
				igroup++;
			} else {
				for (l = 0;l < nmatches[k];l++) {
					kp = matches[9 * k + l];
					checkearly = object[kp];
					if (checkearly >= 0) {
						while (mapgroup[checkearly] != checkearly) {
							tmpearly = mapgroup[checkearly];
							mapgroup[checkearly] = minearly;
							checkearly = tmpearly;
						}
						mapgroup[checkearly] = minearly;
					}
				}
				for (l = 0;l < nmatches[k];l++) {
					kp = matches[9 * k + l];
					object[kp] = minearly;
				}
			}
		}
	}

	ngroups = 0;
	for (i = 0;i < nx*ny;i++) {
		if (mapgroup[i] >= 0) {
			if (mapgroup[i] == i) {
				mapgroup[i] = ngroups;
				ngroups++;
			} else {
				mapgroup[i] = mapgroup[mapgroup[i]];
			}
		}
	}

	for (i = 0;i < nx*ny;i++)
		object[i] = mapgroup[object[i]];

	for (i = 0;i < nx*ny;i++)
		mapgroup[i] = -1;
	igroup = 0;
	for (k = 0;k < nx*ny;k++) {
		if (image[k] > 0 && mapgroup[object[k]] == -1) {
			mapgroup[object[k]] = igroup;
			igroup++;
		}
	}

	for (i = 0;i < nx*ny;i++)
		if (image[i] > 0)
			object[i] = mapgroup[object[i]];
		else
			object[i] = -1;

	FREEVEC(matches);
	FREEVEC(nmatches);
	FREEVEC(mapgroup);

	return (1);
} /* end dfind */
