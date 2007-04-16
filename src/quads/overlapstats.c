/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

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
#include "starkd.h"
#include "xylist.h"
#include "verify.h"

static const char* OPTIONS = "hi:s:S:r:f:F:X:Y:";

static void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s\n"
			"   -i <input matchfile>\n"
			"   (-f <field (xyls) file>  OR  -F <field (xyls) template>)\n"
			"     [-X <x column name>]\n"
			"     [-Y <y column name>]\n"
			"   (-s <star kdtree>  OR  -S <star kdtree template>)\n"
			"   -r <overlap radius (arcsec)>\n"
			"\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];

	char* infn = NULL;
	char* xylsfn = NULL;
	char* starfn = NULL;
	char* xylstemplate = NULL;
	char* startemplate = NULL;
	char* xname = NULL;
	char* yname = NULL;
	matchfile* matchin;
	startree* starkd;
	xylist* xyls;
	double overlap_rad = 0.0;
	double overlap_d2;
	double* fielduv = NULL;

	int lasthp, lastff;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'X':
			xname = optarg;
			break;
		case 'Y':
			yname = optarg;
			break;
		case 'f':
			xylsfn = optarg;
			break;
		case 'F':
			xylstemplate = optarg;
			break;
		case 'i':
			infn = optarg;
			break;
		case 's':
			starfn = optarg;
			break;
		case 'S':
			startemplate = optarg;
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

	if (!infn || !(starfn || startemplate) || !(xylsfn || xylstemplate)) {
		fprintf(stderr, "Must specify input and output match files, star kdtree, and field filenames.\n");
		printHelp(progname);
		exit(-1);
	}

	if (overlap_rad == 0.0) {
		fprintf(stderr, "Must specify overlap radius and threshold.\n");
		printHelp(progname);
		exit(-1);
	}

	overlap_d2 = arcsec2distsq(overlap_rad);

	matchin = matchfile_open(infn);
	if (!matchin) {
		fprintf(stderr, "Failed to read input matchfile %s.\n", infn);
		exit(-1);
	}

	if (starfn) {
		starkd = startree_open(starfn);
		if (!starkd) {
			fprintf(stderr, "Failed to open star kdtree %s.\n", starfn);
			exit(-1);
		}
	} else
		starkd = NULL;

	if (xylsfn) {
		xyls = xylist_open(xylsfn);
		if (!xyls) {
			fprintf(stderr, "Failed to open xylist %s.\n", xylsfn);
			exit(-1);
		}
		if (xname)
			xyls->xname = xname;
		if (yname)
			xyls->yname = yname;
	} else
		xyls = NULL;

	fprintf(stderr, "overlap_dists=[");

	lasthp = lastff = -1;

	for (;;) {
		int NF;
		MatchObj* mo;
		char fn[256];
		int match;
		int unmatch;
		int conflict;
		dl* dists2;
		int j;

		mo = matchfile_buffered_read_match(matchin);
		if (!mo)
			break;
		if (!mo->transform_valid) {
			printf("MatchObject transform isn't valid.\n");
			continue;
		}

		if (startemplate && (mo->healpix != lasthp)) {
			if (starkd)
				startree_close(starkd);
			sprintf(fn, startemplate, mo->healpix);
			starkd = startree_open(fn);
			if (!starkd) {
				fprintf(stderr, "Failed to open star kdtree from file %s.\n", fn);
				exit(-1);
			}
			lasthp = mo->healpix;
		}

		if (xylstemplate && (mo->fieldfile != lastff)) {
			if (xyls)
				xylist_close(xyls);
			sprintf(fn, xylstemplate, mo->fieldfile);
			xyls = xylist_open(fn);
			if (!xyls) {
				fprintf(stderr, "Failed to open xylist file %s.\n", fn);
				exit(-1);
			}
			if (xname)
				xyls->xname = xname;
			if (yname)
				xyls->yname = yname;
			lastff = mo->fieldfile;
		}

		NF = xylist_n_entries(xyls, mo->fieldnum);
		if (NF <= 0) {
			printf("Failed to get field %i.\n", mo->fieldnum);
			continue;
		}
		fielduv = realloc(fielduv, NF * 2 * sizeof(double));
		xylist_read_entries(xyls, mo->fieldnum, 0, NF, fielduv);

		dists2 = dl_new(256);

		verify_hit(starkd->tree, mo, fielduv, NF, overlap_d2,
				   &match, &unmatch, &conflict, NULL, dists2, NULL);
		printf("Overlap: %g\n", mo->overlap);

		for (j=0; j<dl_size(dists2); j++)
			fprintf(stderr, "%g, ", distsq2arcsec(dl_get(dists2, j)));

		dl_free(dists2);
	}

	fprintf(stderr, "];\n");

	free(fielduv);

	if (xyls)
		xylist_close(xyls);
	matchfile_close(matchin);
	if (starkd)
		startree_close(starkd);
	return 0;
}
