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

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "xylist.h"
#include "donuts.h"
#include "fitsioutils.h"
#include "qfits.h"
#include "boilerplate.h"

char* OPTIONS = "hi:X:Y:o:d:t:";

void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "\nUsage: %s [options]\n"
			"   -i <input-xylist-filename>\n"
			"     [-X <x-column-name>]\n"
			"     [-Y <y-column-name>]\n"
			"   -o <output-xylist-filename>\n"
			"   -d <donut-distance-pixels>\n"
			"   -t <donut-threshold>\n",
			progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];

	char* infname = NULL;
	char* outfname = NULL;
	char* xcol = NULL;
	char* ycol = NULL;
	xylist* xyls = NULL;
	xylist* xylsout = NULL;
	int i, N;
	double* fieldxy = NULL;
	double donut_dist = 5.0;
	double donut_thresh = 0.25;
	char val[64];

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'd':
			donut_dist = atof(optarg);
			break;
		case 't':
			donut_thresh = atof(optarg);
			break;
		case 'X':
			xcol = optarg;
			break;
		case 'Y':
			ycol = optarg;
			break;
		case 'i':
			infname = optarg;
			break;
		case 'o':
			outfname = optarg;
			break;
		default:
		case 'h':
			printHelp(progname);
			exit(0);
		}
	}
	if (!infname || !outfname) {
		printHelp(progname);
		exit(-1);
	}

	xyls = xylist_open(infname);
	if (!xyls) {
		fprintf(stderr, "Failed to read xylist from file %s.\n", infname);
		exit(-1);
	}
	if (xcol)
		xyls->xname = xcol;
	if (ycol)
		xyls->yname = ycol;

	xylsout = xylist_open_for_writing(outfname);
	if (!xylsout) {
		fprintf(stderr, "Failed to open xylist file %s for writing.\n", outfname);
		exit(-1);
	}
	if (xcol)
		xylsout->xname = xcol;
	if (ycol)
		xylsout->yname = ycol;

	qfits_header_add(xylsout->header, "HOMERED", "T", "This file has been homered (donuts removed)", NULL);
	sprintf(val, "%f", donut_dist);
	qfits_header_add(xylsout->header, "HMR_DST", val, "Donut radius (pixels)", NULL);
	sprintf(val, "%f", donut_thresh);
	qfits_header_add(xylsout->header, "HMR_THR", val, "Donut threshold (fraction)", NULL);

	boilerplate_add_fits_headers(xylsout->header);
	qfits_header_add(xylsout->header, "HISTORY", "This file was created by the program \"homer\".", NULL, NULL);
	qfits_header_add(xylsout->header, "HISTORY", "homer command line:", NULL, NULL);
	fits_add_args(xylsout->header, argv, argc);
	qfits_header_add(xylsout->header, "HISTORY", "(end of homer command line)", NULL, NULL);
	qfits_header_add(xylsout->header, "HISTORY", "** homer: history from input xyls:", NULL, NULL);
	fits_copy_all_headers(xyls->header, xylsout->header, "HISTORY");
	qfits_header_add(xylsout->header, "HISTORY", "** homer end of history from input xyls.", NULL, NULL);
	qfits_header_add(xylsout->header, "COMMENT", "** homer: comments from input xyls:", NULL, NULL);
	fits_copy_all_headers(xyls->header, xylsout->header, "COMMENT");
	qfits_header_add(xylsout->header, "COMMENT", "** homer: end of comments from input xyls.", NULL, NULL);

	if (xylist_write_header(xylsout)) {
		fprintf(stderr, "Failed to write xyls header.\n");
		exit(-1);
	}

	N = xyls->nfields;
	for (i=0; i<N; i++) {
		int nf;
		int NF;
		printf("Field %i.\n", i);
		NF = xylist_n_entries(xyls, i);
		fieldxy = realloc(fieldxy, NF * 2 * sizeof(double));
		xylist_read_entries(xyls, i, 0, NF, fieldxy);
		nf = NF;
		detect_donuts(i, fieldxy, &nf, donut_dist, donut_thresh);
		if (nf != NF)
			printf("Found donuts! %i -> %i objs.\n", NF, nf);
		if (xylist_write_new_field(xylsout) ||
			xylist_write_entries(xylsout, fieldxy, nf) ||
			xylist_fix_field(xylsout)) {
			fprintf(stderr, "Failed to write xyls field.\n");
			exit(-1);
		}
		fflush(NULL);
	}

	free(fieldxy);

	if (xylist_fix_header(xylsout) ||
		xylist_close(xylsout)) {
		fprintf(stderr, "Failed to close xyls.\n");
		exit(-1);
	}

	xylist_close(xyls);

	return 0;
}

