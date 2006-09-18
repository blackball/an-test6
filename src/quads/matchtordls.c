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
#include "rdlist.h"
#include "xylist.h"

char* OPTIONS = "hx:m:e:r:X:Y:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options]\n"
			"   -x <xylist-filename>\n"
			"   [-X <x-column-name>]\n"
			"   [-Y <y-column-name>]\n"
			"   -m <match-filename>\n"
			"   [-e <matchfile-entry-number>] (default 0)\n"
			"   -r <rdlist-output-filename>\n",
			progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];

	char* matchfname = NULL;
	char* xyfname = NULL;
	char* rdfname = NULL;
	char* xcol = NULL;
	char* ycol = NULL;
	int entry = 0;
	matchfile* mf = NULL;
	xylist* xyls = NULL;
	rdlist* rdls = NULL;
	MatchObj mo;
	int i, N;
	double* xy;
	double* rd;
	double xyz[3];

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'X':
			xcol = optarg;
			break;
		case 'Y':
			ycol = optarg;
			break;
		case 'e':
			entry = atoi(optarg);
			break;
		case 'm':
			matchfname = optarg;
			break;
		case 'r':
			rdfname = optarg;
			break;
		case 'x':
			xyfname = optarg;
			break;
		default:
		case 'h':
			printHelp(progname);
			exit(0);
		}
	}
	if (!matchfname || !xyfname || !rdfname) {
		printHelp(progname);
		exit(-1);
	}

	fprintf(stderr, "Reading matchfile from %s ...\n", matchfname);
	mf = matchfile_open(matchfname);
	if (!mf) {
		fprintf(stderr, "Failed to open matchfile from file %s .\n", matchfname);
		exit(-1);
	}

	xyls = xylist_open(xyfname);
	if (!xyls) {
		fprintf(stderr, "Failed to read xylist from file %s.\n", xyfname);
		exit(-1);
	}
	if (xcol)
		xyls->xname = xcol;
	if (ycol)
		xyls->yname = ycol;

	rdls = rdlist_open_for_writing(rdfname);
	if (!rdls) {
		fprintf(stderr, "Failed to open rdlist for writing to file %s.\n", rdfname);
		exit(-1);
	}

	memset(&mo, 0, sizeof(MatchObj));
	if (matchfile_read_matches(mf, &mo, entry, 1)) {
		fprintf(stderr, "Failed to read match number %i.\n", entry);
		exit(-1);
	}

	if (!mo.transform_valid) {
		fprintf(stderr, "The transform_valid flag is not set.");
		exit(-1);
	}

	printf("Field file %i.\n", mo.fieldfile);
	printf("Field number %i.\n", mo.fieldnum);

	N = xylist_n_entries(xyls, mo.fieldnum);
	xy = malloc(N * 2 * sizeof(double));
	rd = malloc(N * 2 * sizeof(double));

	if (xylist_read_entries(xyls, mo.fieldnum, 0, N, xy)) {
		fprintf(stderr, "Failed to read xylist entries.\n");
		exit(-1);
	}

	for (i=0; i<N; i++) {
		image_to_xyz(xy[i*2], xy[i*2+1], xyz, mo.transform);
		rd[i*2]   = rad2deg(xy2ra(xyz[0], xyz[1]));
		rd[i*2+1] = rad2deg(z2dec(xyz[2]));
	}

	if (rdlist_write_header(rdls) ||
		rdlist_write_new_field(rdls) ||
		rdlist_write_entries(rdls, rd, N) ||
		rdlist_fix_field(rdls) ||
		rdlist_fix_header(rdls) ||
		rdlist_close(rdls)) {
		fprintf(stderr, "Failed to write RDLS.\n");
		exit(-1);
	}

	free(xy);
	free(rd);

	matchfile_close(mf);
	xylist_close(xyls);

	return 0;
}



