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

// This file gets #included in dfind.c, with
// DFIND2 and IMGTYPE defined appropriately.
// I did this so that we can handle int* and
// unsigned char* images using the same code.


int DFIND2(IMGTYPE* image,
           int nx,
           int ny,
           int *object) {
	int ix, iy, i;
	int maxgroups = initial_max_groups;
	short unsigned int *equivs = malloc(sizeof(short unsigned int) * maxgroups);
	short unsigned int *number;
	int maxlabel = 0;
	short unsigned int maxcontiguouslabel = 0;

	/* Keep track of 'on' pixels to avoid later rescanning */
	int num_on_pixels = 0;
	int max_num_on_pixels = 100;
	int **on_pixels = malloc(sizeof(int*)*max_num_on_pixels);

	/* Find blobs and track equivalences */
	for (iy = 0; iy < ny; iy++) {
		for (ix = 0; ix < nx; ix++) {
			int thislabel, thislabelmin;

			object[nx*iy+ix] = -1;
			if (!image[nx*iy+ix])
				continue;

			/* Store location of each 'on' pixel. May be inefficient on 64 bit with large ptrs */
			// FIXME switch to a blocklist!!!!
			if (num_on_pixels >= max_num_on_pixels) {
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
				// FIXME this part should become uf_new_group()
				if (maxlabel >= maxgroups) {
					maxgroups *= 2;
					equivs = realloc(equivs, sizeof(short unsigned int) * maxgroups);
					assert(equivs);
				}
				object[nx*iy+ix] = maxlabel;
				equivs[maxlabel] = maxlabel;
				maxlabel++;

				if (maxlabel == 0xFFFF) {
					fprintf(stderr, "simplexy: ERROR: Exceeded labels. Shrink your image.\n");
					exit(1);
					// FIXME before moving to full ints, we could also try
					// collapsing maxlabels.
				}
				// to here
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
						thislabelmin = newlabelmin;
						equivs[oldlabelmin] = newlabelmin;
						equivs[thislabel] = newlabelmin;
						/* Update other pixel too */
						object[nx*(iy-1) + i] = newlabelmin;
					} 
				}
			}
			object[nx*iy + ix] = thislabelmin;
		}
	}

	/* Re-label the groups */
	number = malloc(sizeof(short unsigned int) * maxlabel);
	assert(number);
	for (i = 0; i < maxlabel; i++)
		number[i] = 0xFFFF;
	for (i=0; i<num_on_pixels; i++) {
		int minlabel = collapsing_find_minlabel(*on_pixels[i], equivs);
		if (number[minlabel] == 0xFFFF)
			number[minlabel] = maxcontiguouslabel++;
		*on_pixels[i] = number[minlabel];
	}

	free(equivs);
	free(number);
	free(on_pixels);
	return 1;
}
