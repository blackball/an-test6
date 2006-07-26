#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "matchfile.h"
#include "kdtree.h"
#include "kdtree_io.h"
#include "kdtree_fits_io.h"

static const char* OPTIONS = "hi:o:s:r:t:f:";

static void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s\n"
			"   -i <input matchfile>\n"
			"   -o <output matchfile>\n"
			"   -s <star kdtree>\n"
			"   -r <overlap radius (arcsec)>\n"
			"   -t <overlap threshold>\n"
			"   -f <min field objects required>\n"
			"\n", progname);
}

static double overlap_thresh = 0.0;
static double overlap_rad = 0.0;
static int min_ninfield = 0;

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];

	/*
	  char** inputfiles = NULL;
	  int ninputfiles = 0;
	  int i;
	*/
	char* infn = NULL;
	char* outfn = NULL;
	char* starfn = NULL;
	matchfile* matchin;
	matchfile* matchout;
	kdtree_t* startree;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'i':
			infn = optarg;
			break;
		case 'o':
			outfn = optarg;
			break;
		case 's':
			starfn = optarg;
			break;
		case 't':
			overlap_thresh = atof(optarg);
			break;
		case 'f':
			min_ninfield = atoi(optarg);
			break;
		case 'r':
			overlap_rad = atof(optarg);
			break;
		case 'h':
		default:
			printHelp(progname);
			exit(-1);
		}
	}

	if (!infn || !outfn || !starfn) {
		fprintf(stderr, "Must specify input, output, and star kdtree filenames.\n");
		printHelp(progname);
		exit(-1);
	}

	if (overlap_rad == 0.0 || overlap_thresh == 0.0) {
		fprintf(stderr, "Must specify overlap radius and threshold.\n");
		printHelp(progname);
		exit(-1);
	}

	matchin = matchfile_open(infn);
	if (!matchin) {
		fprintf(stderr, "Failed to read input matchfile %s.\n", infn);
		exit(-1);
	}

	startree = kdtree_fits_read_file(starfn);
	if (!startree) {
		fprintf(stderr, "Failed to open star kdtree %s.\n", starfn);
		exit(-1);
	}

	matchout = matchfile_open_for_writing(outfn);
	if (!matchout) {
		fprintf(stderr, "Failed to open output matchfile %s.\n", outfn);
		exit(-1);
	}
	if (matchfile_write_header(matchout)) {
		fprintf(stderr, "Failed to write output matchfile header.\n");
		exit(-1);
	}

	for (;;) {
		double xyz[3];
		double t1[3];
		double t2[3];
		double pu[3];
		double pv[3];
		double u1, u2, v1, v2;
		double theta1, theta2;
		double diff;
		double ra, dec;
		double R, D, T, S, P;
		double len1;
		double len2;
		double r2;
		kdtree_qres_t* res;
		MatchObj* mo;
		mo = matchfile_buffered_read_match(matchin);
		if (!mo)
			break;
		if (!mo->transform_valid) {
			printf("MatchObject transform isn't valid.\n");
			continue;
		}

		// find the field center
		star_midpoint(xyz, mo->sMin, mo->sMax);
		// find the field radius
		r2 = distsq(xyz, mo->sMin, 3);
		r2 *= 1.02;
		// find stars in that region
		res = kdtree_rangesearch_nosort(startree, xyz, r2);
		if (!res->nres) {
			printf("no stars found.\n");
			//kdtree_free_query(res);
			//continue;
		}

		// convert the transformation matrix into RA, DEC, theta, scale.
		xyz[0] = mo->transform[2];
		xyz[1] = mo->transform[5];
		xyz[2] = mo->transform[8];
		normalize_3(xyz);
		xyz2radec(xyz[0], xyz[1], xyz[2], &ra, &dec);

		// scale is the mean of the lengths of t1 and t2.
		t1[0] = mo->transform[0];
		t1[1] = mo->transform[3];
		t1[2] = mo->transform[6];
		t2[0] = mo->transform[1];
		t2[1] = mo->transform[4];
		t2[2] = mo->transform[7];
		len1 = sqrt(t1[0]*t1[0] + t1[1]*t1[1] + t1[2]*t1[2]);
		len2 = sqrt(t2[0]*t2[0] + t2[1]*t2[1] + t2[2]*t2[2]);
		S = (len1 + len2) * 0.5;

		// we get theta by projecting pu=[1 0 0] and pv=[0 1 0] through R*D*P
		// and computing the projections of t1, t2 on these.
		R = M_PI_2 - ra;
		D = M_PI_2 - dec;
		pu[0] = cos(R);
		pu[1] = -sin(R);
		pu[2] = 0.0;
		pv[0] = cos(D) * sin(R);
		pv[1] = cos(D) * cos(R);
		pv[2] = -sin(D);
		// note, pu and pv have length 1.

		u1 = t1[0]*pu[0] + t1[1]*pu[1] + t1[2]*pu[2];
		v1 = t1[0]*pv[0] + t1[1]*pv[1] + t1[2]*pv[2];
		u2 = t2[0]*pu[0] + t2[1]*pu[1] + t2[2]*pu[2];
		v2 = t2[0]*pv[0] + t2[1]*pv[1] + t2[2]*pv[2];

		theta1 = atan2(v1, u1);
		theta2 = atan2(v2, u2);

		diff = fabs(theta1 - theta2);
		if (diff > M_PI)
			diff = 2.0*M_PI - diff;

		printf("RA,DEC=(%g, %g) degrees, scale=%g arcsec/pixel,",
			   rad2deg(ra), rad2deg(dec), S * 180/M_PI * 60 * 60);
		printf("theta1=%g, theta2=%g.\n",
			   rad2deg(theta1), rad2deg(theta2));

		// if they're not within 10 degrees of being 90 degrees apart,
		// warn.
		if (fabs(rad2deg(diff) - 90) > 10) {
			printf("warning, theta1 and theta2 should be nearly orthogonal.\n");
		}

		T = theta1;

		// parity: is theta2 90 degrees clockwise or anti-clockwise of theta1?
		diff = theta2 - theta1;
		if (diff < -M_PI)
			diff += 2.0 * M_PI;
		if (diff > M_PI)
			diff -= 2.0 * M_PI;
		P = (diff > 0.0) ? 1.0 : 0.0;

		printf("Parity=%i\n", (int)P);


		kdtree_free_query(res);
	}


	matchfile_close(matchin);
	kdtree_close(startree);

	if (matchfile_fix_header(matchout) ||
		matchfile_close(matchout)) {
		fprintf(stderr, "Failed to close output matchfile %s.\n", outfn);
		exit(-1);
	}

	return 0;
}

