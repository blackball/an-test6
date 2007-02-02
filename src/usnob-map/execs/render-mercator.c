/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <math.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "starutil.h"
#include "an_catalog.h"
#include "mathutil.h"

#define OPTIONS "hW:H:m:M:"

void printHelp(char* progname) {
	fprintf(stderr, "%s usage:\n"
			"  -W <width>    Width of the output.\n"
			"  -H <height>   Height of the output.\n"
			"  [-m <max-y-val>]: Maximum y value of the projection (default: Pi)\n"
			"  [-M <min-mag>]:    Minimum-magnitude star in the catalog (default: 25).\n"
			"\n"
			"  <input-catalog> ...\n"
			"\n"
			"Writes PPM output on stdout.\n"
			"\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

#define max(a,b) (((a)>(b))?(a):(b))

int main(int argc, char** args) {
	int argchar;
	char* progname = args[0];
	float maxy = M_PI;
	int W = 0, H = 0;
	float xscale, yscale;
	float yoffset;
	float* redimg;
	float* blueimg;
	float* nimg;
	int i, j;
	float minmag = 25.0;
	time_t start;

	start = time(NULL);

	while ((argchar = getopt(argc, args, OPTIONS)) != -1)
		switch (argchar) {
		case 'h':
			printHelp(progname);
			exit(0);
		case 'M':
			minmag = atof(optarg);
			break;
		case 'W':
			W = atoi(optarg);
			break;
		case 'H':
			H = atoi(optarg);
			break;
		case 'm':
			maxy = atof(optarg);
			break;
		}

	if (!W || !H) {
		printHelp(progname);
		exit(-1);
	}

	// A.N catalog RA,DEC entries are in degrees.
	//xscale = 2.0 * M_PI / (float)W;
	xscale = (float)W / 360.0;
	yscale = (float)H / (2.0 * maxy);
	yoffset = (float)H / 2.0;

	redimg  = calloc(W*H, sizeof(float));
	blueimg = calloc(W*H, sizeof(float));
	nimg    = calloc(W*H, sizeof(float));

	for (; optind<argc; optind++) {
		char* fn;
		an_catalog* ancat;
		an_entry* entry;
		int oob = 0;
		int lastgrass = 0;

		fn = args[optind];
		ancat = an_catalog_open(fn);
		if (!ancat) {
			fprintf(stderr, "Failed to open Astrometry.net catalog %s\n", fn);
			exit(-1);
		}
		fprintf(stderr, "Reading %i entries for catalog file %s.\n", ancat->nentries, fn);

		for (j=0; j<ancat->nentries; j++) {
			int x, y;
			int grass;
			float vertscale;
			entry = an_catalog_read_entry(ancat);
			if (!entry)
				break;

			grass = (j * 80 / ancat->nentries);
			if (grass != lastgrass) {
				fprintf(stderr, ".");
				fflush(stderr);
				lastgrass = grass;
			}

			assert(entry->ra >= 0.0);
			assert(entry->ra < 360.0);

			/**
			   Note that "y" is defined here such that the top of the image
			   (as viewed on screen or read from file) is y=0 and increases
			   toward the bottom of the image.
			 */
			y = (int)rint(yoffset + yscale * asinh(tan(deg2rad(entry->dec))));

			if ((y < 0) || (y >= H)) {
				oob++;
				continue;
			}

			x = (int)rint(xscale * entry->ra);

			// correct for the distortion of the Mercator projection:
			// high-latitude stars project to low-density areas in
			// the image, so correct by the vertical scaling of the
			// projection: sec(dec) = 1/cos(dec)
			vertscale = 1.0 / cos(deg2rad(entry->dec));

			for (i=0; i<entry->nobs; i++) {
				bool red = FALSE, blue = FALSE, ir = FALSE;
				float flux;
				an_observation* ob = entry->obs + i;
				switch (ob->catalog) {
				case AN_SOURCE_USNOB:
					switch (ob->band) {
					case 'J':
					case 'O':
						blue = TRUE;
						break;
					case 'E':
					case 'F':
						red = TRUE;
						break;
					case 'N':
						ir = TRUE;
						break;
					}
					break;
				case AN_SOURCE_TYCHO2:
					switch (ob->band) {
					case 'B':
						blue = TRUE;
						break;
					case 'V':
						red = TRUE;
						break;
					case 'H':
						blue = TRUE;
						red = TRUE;
						break;
					}
					break;
				}

				flux = exp(-ob->mag) * vertscale;
				if (red)
					redimg[y * W + x] += flux;
				if (blue)
					blueimg[y * W + x] += flux;
				if (ir)
					nimg[y * W + x] += flux;
			}
		}
		fprintf(stderr, "\n");
		an_catalog_close(ancat);
	}

	fprintf(stderr, "Rendering image...\n");
	{
		float rmax, bmax, nmax, minval;
		// DEBUG
		float rmin, bmin, nmin;
		float rscale, bscale, nscale;
		float offset;
		int i;
		minval = exp(-minmag);
		offset = -minmag;
		rmax = bmax = nmax = -1e300;
		rmin = bmin = nmin = 1e300;
		for (i=0; i<(W*H); i++)
			if (redimg[i] > rmax)
				rmax = redimg[i];
		for (i=0; i<(W*H); i++)
			if (blueimg[i] > bmax)
				bmax = blueimg[i];
		for (i=0; i<(W*H); i++)
			if (nimg[i] > nmax)
				nmax = nimg[i];

		/**/
		// DEBUG
		for (i=0; i<(W*H); i++)
			if (redimg[i] < rmin)
				rmin = redimg[i];
		for (i=0; i<(W*H); i++)
			if (blueimg[i] < bmin)
				bmin = blueimg[i];
		for (i=0; i<(W*H); i++)
			if (nimg[i] < nmin)
				nmin = nimg[i];
		fprintf(stderr, "R range [%g, %g]\n", (float)rmin, (float)rmax);
		fprintf(stderr, "B range [%g, %g]\n", (float)bmin, (float)bmax);
		fprintf(stderr, "N range [%g, %g]\n", (float)nmin, (float)nmax);

		/*
		  fprintf(stderr, "Rmax %g\n", (float)rmax);
		  fprintf(stderr, "Bmax %g\n", (float)bmax);
		  fprintf(stderr, "Nmax %g\n", (float)nmax);
		*/

		rscale = 255.0 / (log(rmax) - offset);
		bscale = 255.0 / (log(bmax) - offset);
		nscale = 255.0 / (log(nmax) - offset);
		printf("P6 %d %d %d\n", W, H, 255);
		for (i=0; i<(W*H); i++) {
			unsigned char pix;
			pix = (log(max(redimg[i], minval)) - offset) * rscale;
			putc(pix, stdout);
			pix = (log(max(blueimg[i], minval)) - offset) * bscale;
			putc(pix, stdout);
			pix = (log(max(nimg[i], minval)) - offset) * nscale;
			putc(pix, stdout);
		}
	}

	fprintf(stderr, "That took %i seconds.\n", (int)(time(NULL) - start));

	free(redimg);
	free(blueimg);
	free(nimg);

	return 0;
}


