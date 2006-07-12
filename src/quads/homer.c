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

char* OPTIONS = "hi:X:Y:o:d:t:"; //f:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options]\n"
			"   -i <input-xylist-filename>\n"
			"     [-X <x-column-name>]\n"
			"     [-Y <y-column-name>]\n"
			//"     [-f <field-number>]\n"
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
	//int fieldnum = -1;
	xylist* xyls = NULL;
	xylist* xylsout = NULL;
	int i, N;
	double* fieldxy = NULL;
	double donut_dist = 5.0;
	double donut_thresh = 0.25;

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
			/*
			  case 'f':
			  fieldnum = atoi(optarg);
			  break;
			*/
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
		detect_donuts(i, &fieldxy, &nf, donut_dist, donut_thresh);
		if (nf != NF)
			fprintf(stderr, "Found donuts! %i -> %i objs.\n", NF, nf);
		if (xylist_write_new_field(xylsout) ||
			xylist_write_entries(xylsout, fieldxy, nf) ||
			xylist_fix_field(xylsout)) {
			fprintf(stderr, "Failed to write xyls field.\n");
			exit(-1);
		}
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

