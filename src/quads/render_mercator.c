#include <math.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "fileutil.h"
#include "starutil.h"
#include "an_catalog.h"
#include "mathutil.h"

#define OPTIONS "hW:H:m:"

void printHelp(char* progname) {
	fprintf(stderr, "%s usage:\n"
			"  -W <width>    Width of the output.\n"
			"  -H <height>   Height of the output.\n"
			"  [-m <max-y-val>] Maximum y value of the projection (default: Pi)\n"
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
	float* redimg;
	float* blueimg;
	float* nimg;
	int i, j;

	while ((argchar = getopt(argc, args, OPTIONS)) != -1)
		switch (argchar) {
		case 'h':
			printHelp(progname);
			exit(0);
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

				flux = exp(ob->mag);
				if (red)
					redimg[y * W + x] += flux;
				if (blue)
					blueimg[y * W + x] += flux;
				if (ir)
					nimg[y * W + x] += flux;
			}
		}

		an_catalog_close(ancat);
	}

	{
		float rmax, bmax, nmax, minval;
		int i;
		minval = exp(-25);
		rmax = bmax = nmax = minval;
		for (i=0; i<(W*H); i++)
			if (redimg[i] > rmax)
				rmax = redimg[i];
		for (i=0; i<(W*H); i++)
			if (blueimg[i] > bmax)
				bmax = blueimg[i];
		for (i=0; i<(W*H); i++)
			if (nimg[i] > nmax)
				nmax = nimg[i];

		printf("P6 %d %d %d\n", W, H, 255);
		for (i=0; i<(W*H); i++) {
			unsigned char pix;
			pix = log(max(redimg[i], minval) / rmax);
			putc(pix, stdout);
			pix = log(max(blueimg[i], minval) / bmax);
			putc(pix, stdout);
			pix = log(max(nimg[i], minval) / nmax);
			putc(pix, stdout);
		}
	}

	return 0;
}


