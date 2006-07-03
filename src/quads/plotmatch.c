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

char* OPTIONS = "hi:m:e:n:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options]\n"
			"   -i <index-basename>\n"
			"   -m <matchfile>\n"
			"   [-e <matchfile-entry-number>] (default 0)\n"
			"   [-n <image-size>] (default 1000)\n",
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
	int N = 1000;
	matchfile* mf;
	kdtree_t* startree;
	char* fn;
	MatchObj mo;
	double xyz[3];
	double r, radius2;
	kdtree_qres_t* res;
	double* starxy;
	int i;
	double minx, miny, maxx, maxy;
	//double scale;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'e':
			entry = atoi(optarg);
			break;
		case 'n':
			N = atoi(optarg);
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

	if (!matchfname || !indexfname) {
		printHelp(progname);
		exit(-1);
	}

	fprintf(stderr, "Reading matchfile from %s ...\n", matchfname);
	mf = matchfile_open(matchfname);
	if (!mf) {
		fprintf(stderr, "Failed to open matchfile from file %s .\n", matchfname);
		exit(-1);
	}

	fn = mk_streefn(indexfname);
	fprintf(stderr, "Reading star kdtree from %s ...\n", fn);
	startree = kdtree_fits_read_file(fn);
	if (!startree) {
		fprintf(stderr, "Failed to open star kdtree from file %s .\n", fn);
		exit(-1);
	}
	free_fn(fn);

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

	res = kdtree_rangesearch(startree, xyz, radius2);
	fprintf(stderr, "Found %i stars within range.\n", res->nres);

	starxy = malloc(res->nres * 2 * sizeof(double));

	minx = maxx = miny = maxy = 0.0;
	printf("starxy=[");
	for (i=0; i<res->nres; i++) {
		double x, y;
		star_coords(res->results + i*3, xyz, &x, &y);
		starxy[i*2 + 0] = x;
		starxy[i*2 + 1] = y;
		if (x > maxx) maxx = x;
		if (x < minx) minx = x;
		if (y > maxy) maxy = y;
		if (y < miny) miny = y;
		printf("%g,%g;", x, y);
	}
	printf("];\n");
	{
		double x, y;
		star_coords(mo.sMin, xyz, &x, &y);
		printf("minxy=[%g,%g];\n", x, y);
		printf("minx=%g;\n", x);
		printf("miny=%g;\n", y);
		star_coords(mo.sMax, xyz, &x, &y);
		printf("maxxy=[%g,%g];\n", x, y);
		printf("maxx=%g;\n", x);
		printf("maxy=%g;\n", y);
	}
	/*
	  scale = (maxx - minx) > (maxy - miny) ? (maxx - minx) : (maxy - miny);
	  scale = N /
	  for (i=0; i<res->nres; i++) {
	  }
	*/

	kdtree_close(startree);
	matchfile_close(mf);

	return 0;
}
