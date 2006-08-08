#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "matchobj.h"
#include "matchfile.h"
#include "kdtree_fits_io.h"
#include "kdtree_io.h"

char* OPTIONS = "hi:m:e:n:R:D:r:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options]\n"
			"   -i <index-basename>\n"
			"   (-m <matchfile>\n"
			"    [-e <matchfile-entry-number>] (default 0)\n"
			"   OR\n"
			"    -R <ra> -D <dec> -r <radius in arcmin>)\n",
			progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];

	char* matchfname = NULL;
	char* indexfname = NULL;
	int entry = 0;
	matchfile* mf = NULL;
	kdtree_t* startree;
	char* fn;
	double xyz[3];
	double radius2;
	kdtree_qres_t* res;
	//double* starxy;
	int i;
	double minx, miny, maxx, maxy;
	double ra;
	double dec;
	bool ra_set = FALSE;
	bool dec_set = FALSE;
	double radius = 0.0;
	MatchObj mo;
	double *mincorner=NULL, *maxcorner=NULL;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'R':
			ra = atof(optarg);
			ra_set = TRUE;
			break;
		case 'D':
			dec = atof(optarg);
			dec_set = TRUE;
			break;
		case 'r':
			radius = atof(optarg);
			break;
		case 'e':
			entry = atoi(optarg);
			break;
		case 'm':
			matchfname = optarg;
			break;
		case 'i':
			indexfname = optarg;
			break;
		default:
		case 'h':
			printHelp(progname);
			exit(0);
		}
	}

	if (!indexfname) {
		printHelp(progname);
		exit(-1);
	}
	if (!(matchfname || (radius > 0.0 && ra_set && dec_set))) {
		printHelp(progname);
		exit(-1);
	}

	if (matchfname) {
		double r;
		fprintf(stderr, "Reading matchfile from %s ...\n", matchfname);
		mf = matchfile_open(matchfname);
		if (!mf) {
			fprintf(stderr, "Failed to open matchfile from file %s .\n", matchfname);
			exit(-1);
		}
		memset(&mo, 0, sizeof(MatchObj));
		if (matchfile_read_matches(mf, &mo, entry, 1)) {
			fprintf(stderr, "Failed to read match number %i.\n", entry);
			exit(-1);
		}

		xyz[0] = mo.sMin[0] + mo.sMax[0];
		xyz[1] = mo.sMin[1] + mo.sMax[1];
		xyz[2] = mo.sMin[2] + mo.sMax[2];
		// normalize
		r = sqrt(square(xyz[0]) + square(xyz[1]) + square(xyz[2]));
		xyz[0] /= r;
		xyz[1] /= r;
		xyz[2] /= r;
		radius2 =
			square(xyz[0] - mo.sMin[0]) +
			square(xyz[1] - mo.sMin[1]) +
			square(xyz[2] - mo.sMin[2]);

		mincorner = mo.sMin;
		maxcorner = mo.sMax;

	} else {
		ra  = deg2rad(ra);
		dec = deg2rad(dec);
		xyz[0] = radec2x(ra, dec);
		xyz[1] = radec2y(ra, dec);
		xyz[2] = radec2z(ra, dec);
		radius2 = arcsec2distsq(radius * 60.0);
	}

	fn = mk_streefn(indexfname);
	fprintf(stderr, "Reading star kdtree from %s ...\n", fn);
	startree = kdtree_fits_read_file(fn);
	if (!startree) {
		fprintf(stderr, "Failed to open star kdtree from file %s .\n", fn);
		exit(-1);
	}
	free_fn(fn);

	res = kdtree_rangesearch(startree, xyz, radius2);
	fprintf(stderr, "Found %i stars within range.\n", res->nres);

	//starxy = malloc(res->nres * 2 * sizeof(double));

	//minx = maxx = miny = maxy = 0.0;
	printf("starxy=[");
	for (i=0; i<res->nres; i++) {
		double x, y;
		star_coords(res->results + i*3, xyz, &x, &y);
		/*
		  starxy[i*2 + 0] = x;
		  starxy[i*2 + 1] = y;
		*/
		/*
		  if (x > maxx) maxx = x;
		  if (x < minx) minx = x;
		  if (y > maxy) maxy = y;
		  if (y < miny) miny = y;
		*/
		printf("%g,%g;", x, y);
	}
	printf("];\n");
	{
		double x, y;
		double ra, dec;
		if (mincorner) {
			star_coords(mo.sMin, xyz, &x, &y);
			printf("minxy=[%g,%g];\n", x, y);
			printf("minx=%g;\n", x);
			printf("miny=%g;\n", y);
		}
		if (maxcorner) {
			star_coords(mo.sMax, xyz, &x, &y);
			printf("maxxy=[%g,%g];\n", x, y);
			printf("maxx=%g;\n", x);
			printf("maxy=%g;\n", y);
		}
		ra  = rad2deg(xy2ra(xyz[0], xyz[1]));
		dec = rad2deg(z2dec(xyz[2]));
		printf("ra=%g;\n", ra);
		printf("dec=%g;\n", dec);
	}

	kdtree_close(startree);
	if (mf)
		matchfile_close(mf);

	return 0;
}
