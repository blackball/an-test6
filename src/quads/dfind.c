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
#include <assert.h>

#include "simplexy-common.h"

//#include <valgrind/memcheck.h>

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

#define DEBUG_DFIND 0

int initial_max_groups = 50;

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

/* Finds the root of this set (which is the min label) but collapses
 * intermediate labels as it goes. */
inline short int collapsing_find_minlabel(short int label, short int *equivs)
{
	int min;
	min = label;
	while (equivs[min] != min)
		min = equivs[min];
	while (label != min) {
		int next = equivs[label];
		equivs[label] = min;
		label = next;
	}
	return min;
}

int dfind2(int *image,
           int nx,
           int ny,
           int *object) {
	int ix, iy, i;
	int maxgroups = initial_max_groups; // this is how much room we allocate for groups
	short int *equivs = malloc(sizeof(short int) * maxgroups);
	short int *number = malloc(sizeof(short int) * maxgroups);
	short int maxlabel = 0;
	short int maxcontiguouslabel = 0;

	// we keep track of the pixels we hit; pay memory to avoid rescanning
	int num_on_pixels = 0;
	int max_num_on_pixels = 100;
	int **on_pixels = malloc(sizeof(int*)*max_num_on_pixels);

//	printf("simplexy: dfind2: nx=%d, ny=%d\n",nx,ny);

	/* Find blobs and track equivalences */
	for (iy = 0; iy < ny; iy++) {
		for (ix = 0; ix < nx; ix++) {
			int thislabel, thislabelmin;

			object[nx*iy+ix] = -1;
			if (!image[nx*iy+ix])
				continue;

			/* Store location of each 'on' pixel. May be inefficient on 64 bit with large ptrs */
			if (num_on_pixels >= max_num_on_pixels) {
//				printf("simplexy: max on pixels %d; allocating.\n", max_num_on_pixels);
				max_num_on_pixels *= 2;
				on_pixels = realloc(on_pixels, sizeof(int*) * max_num_on_pixels);
				assert(on_pixels);
			}
			on_pixels[num_on_pixels] = object + nx*iy+ix;
			num_on_pixels++;

			if (ix && image[nx*iy+ix-1]) {
				/* Old group */
				object[nx*iy+ix] = object[nx*iy+ix-1];

			} else {
				/* New blob */
				if (maxlabel >= maxgroups) {
					//	printf("simplexy: exceeded %d groups; allocating.\n", maxgroups);
					maxgroups *= 2;
					equivs = realloc(equivs, sizeof(short int) * maxgroups);
					assert(equivs);
					number = realloc(number, sizeof(short int) * maxgroups);
					assert(number);
				}
				object[nx*iy+ix] = maxlabel;
				equivs[maxlabel] = maxlabel;
				maxlabel++;
			}

			thislabel  = object[nx*iy + ix];

			/* Compute minimum equivalence label for this pixel */
			thislabelmin = collapsing_find_minlabel(thislabel, equivs);

			if (iy == 0)
				continue;

			/* Check three pixels above this one which are 'neighbours' */
			for (i = MAX(0, ix - 1); i < MIN(ix + 2, nx); i++) {
				if (image[nx*(iy-1)+i]) {
					int otherlabel = object[nx*(iy-1) + i];

					/* Find min of the other */
					int otherlabelmin = collapsing_find_minlabel(otherlabel, equivs);

					/* Merge groups if necessary */
					if (thislabelmin != otherlabelmin) {
						int oldlabelmin = MAX(thislabelmin, otherlabelmin);
						int newlabelmin = MIN(thislabelmin, otherlabelmin);
						equivs[oldlabelmin] = newlabelmin;
						thislabelmin = newlabelmin;
						equivs[object[nx*iy+ix]] = newlabelmin;
					}
				}
			}
		}
	}

	for (i = 0; i < maxlabel; i++) {
		number[i] = -1;
	}

	/* debug print */
#if DEBUG_DFIND
	for (iy = 0; iy < ny; iy++) {
		for (ix = 0; ix < nx; ix++) {
			int n = object[nx*iy+ix];
			if (n == -1)
				printf("  ");
			else
				printf(" %d", n);
		}
		printf("\n");
	}
	printf("... \n");
#endif

	/* Re-label the groups */
	for (i=0; i<num_on_pixels; i++) {
		int minlabel = collapsing_find_minlabel(*on_pixels[i], equivs);
		if (number[minlabel] == -1) {
			number[minlabel] = maxcontiguouslabel++;
		}
		*on_pixels[i] = number[minlabel];
	}

#if DEBUG_DFIND
	/* debug print */
	for (iy = 0; iy < ny; iy++) {
		for (ix = 0; ix < nx; ix++) {
			int n = object[nx*iy+ix];
			if (n == -1)
				printf(" . ");
			else
				printf("%3d", n);
		}
		printf("\n");
	}
#endif

	free(equivs);
	free(number);
	free(on_pixels);
	return 1;
}

int dfind(int *image,
          int nx,
          int ny,
          int *object) {
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

	if (ngroups == 0)
		goto bail;

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

bail:
	FREEVEC(matches);
	FREEVEC(nmatches);
	FREEVEC(mapgroup);

	return (1);
} /* end dfind */
