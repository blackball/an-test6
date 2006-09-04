#include <math.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "fileutil.h"
#include "starutil.h"
#include "an_catalog.h"
#include "mathutil.h"

#define OPTIONS "hW:H:m:"

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
	double maxy = M_PI;
	int W = 0, H = 0;
	double xscale, yscale;
	double yoffset;
	double* redimg;
	double* blueimg;
	double* nimg;
	int i, j;
	double minmag = 25.0;
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
	//xscale = 2.0 * M_PI / (double)W;
	xscale = (double)W / 360.0;
	yscale = (double)H / (2.0 * maxy);
	yoffset = (double)H / 2.0;

	redimg  = calloc(W*H, sizeof(double));
	blueimg = calloc(W*H, sizeof(double));
	nimg    = calloc(W*H, sizeof(double));

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
			double vertscale;
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
				double flux;
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
		double rmax, bmax, nmax, minval;
		// DEBUG
		double rmin, bmin, nmin;
		double rscale, bscale, nscale;
		double offset;
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
		fprintf(stderr, "R range [%g, %g]\n", (double)rmin, (double)rmax);
		fprintf(stderr, "B range [%g, %g]\n", (double)bmin, (double)bmax);
		fprintf(stderr, "N range [%g, %g]\n", (double)nmin, (double)nmax);

		/*
		  fprintf(stderr, "Rmax %g\n", (double)rmax);
		  fprintf(stderr, "Bmax %g\n", (double)bmax);
		  fprintf(stderr, "Nmax %g\n", (double)nmax);
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


